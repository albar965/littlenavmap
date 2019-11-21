This folder contains two examples files (SSL key and certificate) for running the Little Navmap
webserver in encrypted (i.e. HTTPS, SSL) mode.

The files can be generated on Linux with the command line:
openssl req -x509 -nodes -days 365 -newkey rsa:2048 -keyout my.key -out my.cert

Be aware that these example files do not provide absolute security since the private key is available publicly in the
Little Navmap GIT repository.
