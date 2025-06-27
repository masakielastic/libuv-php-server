<?php
// Simple test to check current behavior

echo "Testing Aurora with simple setup...\n";

// Use existing built extension
if (!extension_loaded('aurora')) {
    echo "Extension not loaded, trying to load...\n";
    exit(1);
}

echo "Aurora extension is loaded!\n";

// Test with a simple handler
aurora_register_handler(function($env, $respond) {
    echo "Handler called!\n";
    var_dump($env);
    return "Test response from PHP handler";
});

echo "Handler registered, starting server...\n";

// Start server with minimal options
aurora_serve(['host' => '127.0.0.1', 'port' => 8080]);
?>