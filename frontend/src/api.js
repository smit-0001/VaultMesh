import axios from 'axios';

// REMOVED: const API_URL = 'http://localhost:8000';

const api = axios.create({
  // No baseURL needed; it will default to current origin (e.g. localhost:5173)
  // and Vite will proxy it to port 8000.
});

api.interceptors.request.use((config) => {
  const token = localStorage.getItem('token');
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

export default api;