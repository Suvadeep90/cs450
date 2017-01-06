#!/bin/bash
#!/bin/sh


gcc war_server.c -o war_server.out
chmod +x war_server.out
./war_server.out $1

