from fastapi import FastAPI, Depends
from sqlalchemy.orm import Session
from sqlalchemy import text
from .database import get_db, engine
from .api import auth_routes, file_routes
from . import models
from fastapi.middleware.cors import CORSMiddleware

# Initialize App
app = FastAPI(title="VaultMesh Control Plane")

origins = [
    "http://localhost:5173",  # React Dev Server
    "http://127.0.0.1:5173",  # React Dev Server (IP)
]

app.add_middleware(
    CORSMiddleware,
    allow_origins=origins,
    allow_credentials=True,
    allow_methods=["*"],  # Allow all methods (GET, POST, etc.)
    allow_headers=["*"],  # Allow all headers
)

# Health Check
@app.get("/")
def read_root():
    return {"status": "VaultMesh System Online", "version": "1.0.0"}

# Database Check Endpoint
@app.get("/health/db")
def test_db_connection(db: Session = Depends(get_db)):
    try:
        # Try to execute a simple query
        result = db.execute(text("SELECT 1"))
        return {"database": "Connected", "response": result.scalar()}
    except Exception as e:
        return {"database": "Error", "details": str(e)}

app.include_router(auth_routes.router)

app.include_router(file_routes.router)

if __name__ == "__main__":
    import uvicorn
    # Run the server
    uvicorn.run("src.main:app", host="0.0.0.0", port=8000, reload=True)