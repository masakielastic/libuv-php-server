<?php
// Aurora HTTP Server Demo Page

?>
<!DOCTYPE html>
<html lang="ja">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Aurora HTTP Server</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            min-height: 100vh;
        }
        .container {
            background: rgba(255, 255, 255, 0.1);
            padding: 30px;
            border-radius: 15px;
            backdrop-filter: blur(10px);
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
        }
        h1 {
            text-align: center;
            font-size: 2.5em;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.3);
        }
        .subtitle {
            text-align: center;
            font-size: 1.2em;
            opacity: 0.9;
            margin-bottom: 30px;
        }
        .info-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            margin: 30px 0;
        }
        .info-card {
            background: rgba(255, 255, 255, 0.1);
            padding: 20px;
            border-radius: 10px;
            border: 1px solid rgba(255, 255, 255, 0.2);
        }
        .info-card h3 {
            margin-top: 0;
            color: #ffd700;
        }
        .features {
            list-style: none;
            padding: 0;
        }
        .features li {
            padding: 5px 0;
            padding-left: 20px;
            position: relative;
        }
        .features li:before {
            content: "✦";
            position: absolute;
            left: 0;
            color: #ffd700;
        }
        .code {
            background: rgba(0, 0, 0, 0.3);
            padding: 15px;
            border-radius: 8px;
            font-family: 'Courier New', monospace;
            margin: 15px 0;
            overflow-x: auto;
        }
        .footer {
            text-align: center;
            margin-top: 30px;
            opacity: 0.8;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🌟 Aurora HTTP Server</h1>
        <p class="subtitle">libuv-powered PHP HTTP server</p>
        
        <div class="info-grid">
            <div class="info-card">
                <h3>📊 Server Info</h3>
                <p><strong>Time:</strong> <?= date('Y-m-d H:i:s') ?></p>
                <p><strong>PHP Version:</strong> <?= PHP_VERSION ?></p>
                <p><strong>Server:</strong> Aurora/0.1.0</p>
                <p><strong>Request Method:</strong> <?= $_SERVER['REQUEST_METHOD'] ?? 'GET' ?></p>
            </div>
            
            <div class="info-card">
                <h3>⚡ Features</h3>
                <ul class="features">
                    <li>libuv event loop</li>
                    <li>llhttp parser</li>
                    <li>Non-blocking I/O</li>
                    <li>TLS/HTTPS support</li>
                    <li>Zero-config setup</li>
                </ul>
            </div>
        </div>

        <div class="info-card">
            <h3>🚀 Quick Start</h3>
            <div class="code">
# Start server (default: 127.0.0.1:8080)
./aurora serve

# Custom host and port
./aurora serve -h 0.0.0.0 -p 3000

# With document root
./aurora serve -d /var/www/html

# Enable HTTPS
./aurora serve -t cert.pem -k key.pem
            </div>
        </div>

        <div class="info-card">
            <h3>🔧 Environment</h3>
            <p><strong>Document Root:</strong> <?= getcwd() ?></p>
            <p><strong>Script:</strong> <?= __FILE__ ?></p>
            <p><strong>Extensions:</strong> 
                <?php 
                $extensions = ['aurora', 'uv', 'openssl'];
                foreach ($extensions as $ext) {
                    echo extension_loaded($ext) ? "✅ $ext " : "❌ $ext ";
                }
                ?>
            </p>
        </div>

        <div class="footer">
            <p>Built with ❤️ for high-performance PHP web serving</p>
        </div>
    </div>
</body>
</html>