# Skip test when run on Travis, as it doesn't support IPv6
if [ $TRAVIS ]; then
    exit 125;
fi

cd sockets && ./testtcpsocketipv6.sh
