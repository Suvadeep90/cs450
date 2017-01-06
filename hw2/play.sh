
if [ $# != 3 ]
then
   echo "Error. Need host and port argument"
fi
 
if [ $1 == 'client' ] 
then
   echo "Running client code" 
   ./client.out $2 $3
fi
 
if [ $1 == 'server' ] 
then
   echo "Running Server on port " $3 
   ./server.out $3
fi