Memory cards rendering system for Thai language learning: the webserver.

## Prerequisites

```
sudo apt install git g++ cmake libmicrohttpd-dev certbot
```

## Buidling

Create SSL certificates:

```
cd thaicards-webserver
mkdir ssl
cd ssl
sudo certbot certonly -d $(hostname) --standalone --agree-tos -m dmitry@kernelgen.org
sudo cp /etc/letsencrypt/live/thaicards/privkey.pem id_rsa.thaicards
sudo cp /etc/letsencrypt/live/thaicards/fullchain.pem id_rsa.thaicards.crt
cd ..
```

Build the server:

```
mkdir build
cd build
cmake ..
make -j12
```

## Deployment

* **Development mode**: deploy the server either on an arbitrary non-default port for development, or on port 443 to have SSL support and redirection from HTTP to HTTPS:

```
sudo ./thaicards-webserver 443
```

* **Production mode**: running webserver with "sudo" is generally very insequre; alternatively, install nginx and configure a proxypass to an unprivileged  local port:

```
sudo apt install nginx
```

```
cat /etc/nginx/sites-available/webserver
server {
    listen 443 ssl;
    listen [::]:443 ssl;
    server_name thaicards.mikush.in;
    access_log off;
    error_log off;
    ssl_certificate /etc/letsencrypt/live/hostname/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/hostname/privkey.pem;

    location / {
      client_max_body_size 0;
      proxy_pass http://localhost:3000;
      proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
      proxy_set_header X-Real-IP $remote_addr;
      proxy_set_header Host $http_host;
      proxy_set_header X-Forwarded-Proto $scheme;
      proxy_max_temp_file_size 0;
      proxy_redirect off;
      proxy_read_timeout 120;
    }
}

# Redirect HTTP requests to HTTPS
server {
    listen 80;
    server_name thaicards.mikush.in;
    return 301 https://$host$request_uri;
}
```

```
sudo systemctl restart nginx
./thaicards-webserver 3000
```

