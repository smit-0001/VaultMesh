-- Enable UUID extension for unique IDs
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- 1. GROUPS (Departments/Teams)
CREATE TABLE groups (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) UNIQUE NOT NULL, -- e.g., "DevOps", "Finance"
    allocated_storage_gb INT DEFAULT 100 -- Default quota for the group
);

-- 2. USERS (Employees)
CREATE TABLE users (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    email VARCHAR(100) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(100),
    group_id INT REFERENCES groups(id) ON DELETE SET NULL,
    role VARCHAR(20) DEFAULT 'USER', -- 'ADMIN' or 'USER'
    is_active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP DEFAULT NOW()
);

-- 3. STORAGE NODES (Where the C++ servers live)
CREATE TABLE storage_nodes (
    id SERIAL PRIMARY KEY,
    hostname VARCHAR(100) NOT NULL, -- e.g., "10.0.0.5" or "storage-node-1"
    port INT DEFAULT 9000,
    status VARCHAR(20) DEFAULT 'ACTIVE', -- 'ACTIVE', 'DOWN'
    total_capacity_bytes BIGINT NOT NULL,
    used_capacity_bytes BIGINT DEFAULT 0
);

-- 4. FILES (Metadata)
CREATE TABLE files (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    filename VARCHAR(255) NOT NULL,
    size_bytes BIGINT NOT NULL,
    owner_id UUID REFERENCES users(id) ON DELETE CASCADE,
    folder_path VARCHAR(500) DEFAULT '/', 
    
    -- UPDATED: Changed from 'storage_node_id' (int) to string to match Python
    storage_node_ip VARCHAR(100), 
    
    -- UPDATED: Renamed from 'physical_path' to 'storage_path'
    storage_path VARCHAR(500) NOT NULL, 
    
    uploaded_at TIMESTAMP DEFAULT NOW()
);

-- Seed Default Data: Create an Admin Account
-- Password is 'admin123' (hashed using bcrypt for demo purposes)
-- In production, you would generate this or force change on first login.
INSERT INTO groups (name) VALUES ('System Admins');

INSERT INTO users (email, password_hash, role, group_id) 
VALUES (
    'admin@vaultmesh.local', 
    '$2b$12$eX6lq.Z6.i6.i6.i6.i6.eX6lq.Z6.i6.i6.i6.i6.eX6lq.Z6.i6', -- Placeholder hash
    'ADMIN',
    1
);