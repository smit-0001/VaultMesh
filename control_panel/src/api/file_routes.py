from fastapi import APIRouter, UploadFile, Depends, HTTPException, File
from fastapi.responses import StreamingResponse
from sqlalchemy.orm import Session
from typing import List
from pydantic import BaseModel
from datetime import datetime
from uuid import UUID  # <--- NEW IMPORT
import socket
import struct
import os
from .. import database, models, auth

CMD_DOWNLOAD = 0x02

router = APIRouter(prefix="/files", tags=["Files"])

# Configuration: Read from Env or default to localhost (for local testing)
STORAGE_NODE_HOST = os.getenv("STORAGE_NODE_HOST", "127.0.0.1")
STORAGE_NODE_PORT = int(os.getenv("STORAGE_NODE_PORT", 9000))

# Protocol Constants (Must match C++ Protocol.h)
MAGIC = 0x56        # 'V'
CMD_UPLOAD = 0x01


# Response Schema (How the data looks in JSON)
class FileResponse(BaseModel):
    id: UUID
    filename: str
    size_bytes: int
    uploaded_at: datetime
    
    class Config:
        from_attributes = True

@router.get("/", response_model=List[FileResponse])
def list_files(
    current_user: models.User = Depends(auth.get_current_user),
    db: Session = Depends(database.get_db)
):
    # Admins see all files, Users see only their own
    if current_user.role == "ADMIN":
        files = db.query(models.File).all()
    else:
        files = db.query(models.File).filter(models.File.owner_id == current_user.id).all()
    
    return files

@router.post("/upload")
def upload_file(
    file: UploadFile = File(...), 
    current_user: models.User = Depends(auth.get_current_user),
    db: Session = Depends(database.get_db)
):
    """
    Synchronous version: Runs in a threadpool to safely handle blocking Sockets.
    """
    
    # 1. Prepare Metadata
    filename = file.filename
    filename_bytes = filename.encode('utf-8')
    
    # Get File Size (Sync way)
    file.file.seek(0, os.SEEK_END)
    file_size = file.file.tell()
    file.file.seek(0) 

    # 2. Connect to C++ Node
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((STORAGE_NODE_HOST, STORAGE_NODE_PORT))
            
            # 3. Send Header
            header = struct.pack('>BBIQ', MAGIC, CMD_UPLOAD, len(filename_bytes), file_size)
            sock.sendall(header)
            
            # 4. Send Filename
            sock.sendall(filename_bytes)
            
            # 5. Stream the File Content
            # We use file.file.read() because we are in a sync function now
            while chunk := file.file.read(4096):
                sock.sendall(chunk)
                
    except ConnectionRefusedError:
        raise HTTPException(status_code=503, detail="Storage Node is unavailable")
    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Transfer failed: {str(e)}")

    # 6. Save Metadata to Database
    new_file = models.File(
        filename=filename,
        size_bytes=file_size,
        owner_id=current_user.id,
        storage_node_ip=f"{STORAGE_NODE_HOST}:{STORAGE_NODE_PORT}",
        storage_path=f"./data/{filename}" 
    )
    
    db.add(new_file)
    db.commit()
    db.refresh(new_file)

    return {"message": "Upload successful", "file_id": str(new_file.id)}

@router.get("/download/{file_id}")
def download_file(
    file_id: str, 
    current_user: models.User = Depends(auth.get_current_user),
    db: Session = Depends(database.get_db)
):
    # 1. Get File Metadata from DB
    file_record = db.query(models.File).filter(models.File.id == file_id).first()
    if not file_record:
        raise HTTPException(status_code=404, detail="File not found")
        
    # Security: Ensure user owns the file (or is admin)
    if str(file_record.owner_id) != str(current_user.id) and current_user.role != "ADMIN":
        raise HTTPException(status_code=403, detail="Not authorized")

    filename = file_record.filename

    # 2. Generator Function (Streams data from C++ to Browser)
    def iterfile():
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.connect((STORAGE_NODE_HOST, STORAGE_NODE_PORT))
                
                # A. Send Request: [Magic][DOWNLOAD][NameLen][0][Filename]
                filename_bytes = filename.encode('utf-8')
                header = struct.pack('>BBIQ', MAGIC, CMD_DOWNLOAD, len(filename_bytes), 0)
                sock.sendall(header)
                sock.sendall(filename_bytes)
                
                # B. Read Response Header (14 bytes)
                resp_header = sock.recv(14)
                if not resp_header or len(resp_header) < 14:
                    raise Exception("Storage node did not respond")
                    
                # Unpack size (We ignore Magic/Cmd for now to keep it simple)
                _, _, _, file_size = struct.unpack('>BBIQ', resp_header)
                
                # C. Stream the Data
                remaining = file_size
                while remaining > 0:
                    chunk_size = 4096 if remaining > 4096 else remaining
                    chunk = sock.recv(chunk_size)
                    if not chunk: 
                        break
                    yield chunk
                    remaining -= len(chunk)
                    
        except Exception as e:
            print(f"Download Error: {e}")
            yield b"" # Stop stream on error

    # 3. Return Streaming Response
    return StreamingResponse(
        iterfile(), 
        media_type="application/octet-stream",
        headers={"Content-Disposition": f"attachment; filename={filename}"}
    )

