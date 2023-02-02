debug=0;
upload=1;

while getopts dn flag
do
    case "${flag}" in
        d) debug=1;;
        n) upload=0;;
    esac
done

if [ $debug -eq 1 ]
then
    arduino-cli compile -v --fqbn Intel:arc32:arduino_101 --build-property build.extra_flags="-DDEBUG=1" ${PWD##/*}
else
    arduino-cli compile -v --fqbn Intel:arc32:arduino_101 ${PWD##/*}
fi

if [ $upload -eq 1 ]
then
    arduino-cli upload -v -p /dev/ttyACM0 --fqbn Intel:arc32:arduino_101 ${PWD##/*}
fi
