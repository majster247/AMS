// src/components/Home.tsx
import React from 'react';
import './Home.css';
const Home: React.FC = () => {
  return (
    <div>
      <h2>About AMS-OS</h2>
      <p>
        AMS-OS is a simple and lightweight operating system designed for enthusiasts and developers. It's not like another Unix-based system - it's fully written by hand and compiled with its own compiler in its own language.
      </p>
      <h5>Github repo: <a href="https://github.com/majster247/AMS/">AMS-OS official repo</a></h5>


      <h2>Key Features</h2>
      <ul>
        <li>Real-time kernel</li>
        <li>Own filesystem called "ShadowWizard" and bootloader called "Baobab"</li>
        <li>Unique in a world of technology</li>
        <li>16/32/64-bit port (supporting multiple architectures)</li>
        <li>Network & VGA driver (up to 8k 30.0Hz support in pre-alpha, HDMI drivers in the future)</li>
      </ul>

      <br /><br />
      <p>PROJECT SPONSOR:</p>
      <p>Github Sponsor: <iframe src="https://github.com/sponsors/majster247/button" title="Sponsor majster247" height="32" width="114" style={{ border: 0, borderRadius: '6px' }}></iframe></p>
      <p>Patronite: ....</p>
      <p><b>In any issues, problems, or questions in supporting me, write to any e-mail in a contact section</b></p>
    </div>
  );
};

export default Home;
