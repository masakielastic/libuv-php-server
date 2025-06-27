<?php
// Aurora HTTP Server Bootstrap Script

// Parse command line arguments
$options = [];
$args = array_slice($argv, 1);

for ($i = 0; $i < count($args); $i++) {
    switch ($args[$i]) {
        case '--host':
            $options['host'] = $args[++$i] ?? '127.0.0.1';
            break;
        case '--port':
            $options['port'] = (int)($args[++$i] ?? 8080);
            break;
        case '--docroot':
            $options['docroot'] = $args[++$i] ?? getcwd();
            break;
        case '--tls-cert':
            $options['tls_cert'] = $args[++$i] ?? '';
            break;
        case '--tls-key':
            $options['tls_key'] = $args[++$i] ?? '';
            break;
        case '--verbose':
            $options['verbose'] = true;
            break;
    }
}

// Set defaults
$options['host'] = $options['host'] ?? '127.0.0.1';
$options['port'] = $options['port'] ?? 8080;
$options['docroot'] = $options['docroot'] ?? getcwd();
$options['verbose'] = $options['verbose'] ?? false;

if ($options['verbose']) {
    echo "Aurora HTTP Server starting...\n";
    echo "Configuration:\n";
    foreach ($options as $key => $value) {
        if (is_bool($value)) {
            $value = $value ? 'true' : 'false';
        }
        echo "  $key: $value\n";
    }
    echo "\n";
}

// Register default handler
aurora_register_handler(function($env, $respond) use ($options) {
    // Simple file serving logic
    $url = $env['REQUEST_URI'] ?? '/';
    $path = rtrim($options['docroot'], '/') . $url;
    
    if ($url === '/') {
        $path .= '/index.php';
    }
    
    if ($options['verbose']) {
        echo "Request: {$env['REQUEST_METHOD']} $url -> $path\n";
    }
    
    if (file_exists($path) && is_file($path)) {
        if (pathinfo($path, PATHINFO_EXTENSION) === 'php') {
            // Execute PHP file
            ob_start();
            try {
                include $path;
                $content = ob_get_contents();
            } catch (Throwable $e) {
                $content = "PHP Error: " . $e->getMessage();
            } finally {
                ob_end_clean();
            }
            
            $respond(200, ['Content-Type' => 'text/html'], $content);
        } else {
            // Serve static file
            $content = file_get_contents($path);
            $mime_type = 'text/plain';
            
            $ext = pathinfo($path, PATHINFO_EXTENSION);
            switch ($ext) {
                case 'html': case 'htm': $mime_type = 'text/html'; break;
                case 'css': $mime_type = 'text/css'; break;
                case 'js': $mime_type = 'application/javascript'; break;
                case 'json': $mime_type = 'application/json'; break;
                case 'png': $mime_type = 'image/png'; break;
                case 'jpg': case 'jpeg': $mime_type = 'image/jpeg'; break;
                case 'gif': $mime_type = 'image/gif'; break;
                case 'svg': $mime_type = 'image/svg+xml'; break;
            }
            
            $respond(200, ['Content-Type' => $mime_type], $content);
        }
    } else {
        $respond(404, ['Content-Type' => 'text/html'], 
                 "<h1>404 Not Found</h1><p>The requested file was not found.</p>");
    }
});

// Start the server
echo "Aurora HTTP Server listening on http://{$options['host']}:{$options['port']}\n";
echo "Document root: {$options['docroot']}\n";
echo "Press Ctrl+C to stop\n\n";

aurora_serve($options);
