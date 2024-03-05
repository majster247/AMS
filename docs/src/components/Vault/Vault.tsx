import React from "react";
import './Vault.css';
const Vault: React.FC = () => {
    return (
        <div>
           
            <main>
                <div> {/* Wrap the multiple JSX elements inside a parent div */}
                  
                    <p>
                        Official AMS-OS valut with all content of files/versions and other type of content about AMS-OS
                    </p>
                    <ol>
                        <ul>
                            <h3>ASM-OS distributions</h3>
                            <h5>Github download folder: <a href="https://github.com/majster247/AMS/tree/main/docs/download/Vault/AMS">AMS-OS official Vault</a></h5>
                            <li>AMS-OS v0.6.1 [PRE-ALPHA]:
                                <ol>ISO Release: <a href="Vault/AMS/AMS-OSv0.6.1/AMS-OS-0.6.1.iso">Download</a></ol>
                                <ol>BIN Release: <a href="Vault/AMS/AMS-OSv0.6.1/AMS-OS-0.6.1.bin">Download</a></ol>
                                <ol>ZIP Source Code: <a href="Vault/AMS/AMS-OSv0.6.1/AMS-0.6.1.zip">Download</a></ol>
                                <ol>TAR.GZ Source Code: <a href="Vault/AMS/AMS-OSv0.6.1/AMS-0.6.1.tar.gz">Download</a></ol>
                            </li>
                            <li>AMS-OS v0.6.0 [PRE-ALPHA]:
                                <ol>ISO Release: <a href="Vault/AMS/AMS-OSv0.6/AMS-OS-0.6.iso">Download</a></ol>
                                <ol>BIN Release: <a href="Vault/AMS/AMS-OSv0.6/AMS-OS-0.6.bin">Download</a></ol>
                                <ol>ZIP Source Code: <a href="Vault/AMS/AMS-OSv0.6/AMS-Release.zip">Download</a></ol>
                                <ol>TAR.GZ Source Code: <a href="Vault/AMS/AMS-OSv0.6/AMS-Release.tar.gz">Download</a></ol>
                            </li>
                            <li>AMS-OS pre v0.6 old distro: ....</li>
                            <li>AMS-OS AMS32/C before C++ switch:....??</li>
                        </ul>
                    </ol>
                </div>
            </main>

        </div>
    );
};

export default Vault;