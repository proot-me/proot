if [ -z `which ls` ]; then
    exit 125;
fi

${PROOT} -b /etc:/x ls -la /x
