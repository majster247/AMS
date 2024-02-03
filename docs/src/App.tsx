// src/components/MainApp.tsx
import React from 'react';
import { BrowserRouter as Router, Link, Route, Routes } from 'react-router-dom';
import Home from './components/Home/Home';
import Posts from './components/Posts/Posts';
import Contact from './components/Contact/Contact';
import Vault from './components/Vault/Vault';

const MainApp: React.FC = () => {
  return (
    <Router>
      <div>
        <header>
          <h1>AMS-OS</h1>
        </header>

        <nav>
          <Link to="/">Home</Link>
          <Link to="/posts">Posts</Link>
          <Link to="/contact">Contact</Link>
          <Link to="/vault">Vault</Link>
        </nav>

        <main>
          <Routes>
            <Route path="/posts" element={<Posts />} />
            <Route path="/contact" element={<Contact />} />
            <Route path="/vault" element={<Vault />} />
            <Route path="/" element={<Home />} />
          </Routes>
        </main>

       
      </div>
    </Router>
  );
};

export default MainApp;
