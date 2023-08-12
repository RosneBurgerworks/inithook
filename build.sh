#!/usr/bin/env bash
DATA_DIR=${DATA_DIR:-/opt/cathook/data}
DATA_DIR_CATH=${DATA_DIR_CATH:-/opt/cathook/data}
CATH_BINARY_DEST=${CATH_BINARY_DEST:-""}

echo "Data directory                     --> \"$DATA_DIR\""
echo "Data directory relative to cathook --> \"$DATA_DIR_CATH\""
echo "Binary file destination            --> \"$CATH_BINARY_DEST\""
echo ""

read -r -p "Confirm build with these settings? [y/N] " response
if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]
then
	echo ""
	cd obj && cmake -DDataPath=$DATA_DIR_CATH -DCMAKE_BUILD_TYPE=Debug .. && cmake --build . --target cathook -- -j$(grep -c '^processor' /proc/cpuinfo) || { echo -e "\033[1;31m \n \nFailed to compile cathook"; exit 1; }
	if [ "$CATH_BINARY_DEST" != "" ] ; then
		cp bin/libcathook.so $CATH_BINARY_DEST
	fi

	cd .. && ./install-data.sh || { echo -e "\033[1;31m \n \nFailed to update $DATA_DIR directory, trying again as root."; sudo ./install-data.sh; }
	echo -e "\n\n\033[1;34mCathook updated successfully"
else
	exit 1
fi

