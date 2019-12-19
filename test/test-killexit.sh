if [ -z `which setsid` ]; then
    exit 125
fi

cleanup() {
    _code=$?
    trap - INT TERM EXIT
    [ ! -f "${tmpfile-}" ] || rm -f "$tmpfile"
    exit $_code
}
trap cleanup INT TERM EXIT

tmpfile=`mktemp`

# Check that kill on exit option is recognized
${PROOT} --kill-on-exit true

# Check that detached sleep does not block proot
# I.e. in the file we must have "success" first, not "fail"
${PROOT} --kill-on-exit sh -c "setsid sh -c \"sleep 2; echo fail >>$tmpfile\" &"
echo "success" >>$tmpfile
read status_ <$tmpfile
[ "$status_" = success ] || exit 1
