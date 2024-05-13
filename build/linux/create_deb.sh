#!/bin/bash

# Echo all commands and exit on failure
set -e
set -x

# Error checking for required variable APROJECTS
if [ -z "${APROJECTS}" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "${APROJECTS}" ]; then echo "${APROJECTS}" does not exist ; exit 1 ; fi

# load system we are running on
source /etc/lsb-release

############################################################
# Little Navmap ############################################
############################################################

# release build of little navmap
DEPLOY_NAME="Little Navmap"
DEPLOY_DIR="${APROJECTS}/deploy/${DEPLOY_NAME}"

# package information
NAME="littlenavmap"
VERSION=$(head -n1 "${DEPLOY_DIR}/version.txt")
REVISION="1"
ARCH="amd64"

# setup workdir
WORKDIR="${APROJECTS}/deb/${DISTRIB_ID}_${DISTRIB_RELEASE}/${NAME}_${VERSION}-${REVISION}_${ARCH}"
rm -rf "${WORKDIR}" "${WORKDIR}.deb"
mkdir -p "${WORKDIR}"

# copy release
mkdir "${WORKDIR}/opt"
cp -ra "${DEPLOY_DIR}" "${WORKDIR}/opt"

# create meta information
mkdir "${WORKDIR}/DEBIAN"
cat << EOT > "${WORKDIR}/DEBIAN/control"
Package: ${NAME}
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: Daniel Buschke <damage@devloop.de>
Description: Little Navmap
 Little Navmap is a free open source flight planner, navigation tool, moving map, airport search and airport information system
Homepage: https://albar965.github.io
EOT

# add desktop file to global application directory
mkdir -p "${WORKDIR}/usr/share/applications"
cp "${WORKDIR}/opt/${DEPLOY_NAME}/${DEPLOY_NAME}.desktop" "${WORKDIR}/usr/share/applications/${NAME}.desktop"
sed -i "s;YOUR_PATH_TO_LITTLENAVMAP;/opt/${DEPLOY_NAME};g" "${WORKDIR}/usr/share/applications/${NAME}.desktop"

# postinst file
cat << EOT > "${WORKDIR}/DEBIAN/postinst"
ln -s "/opt/${DEPLOY_NAME}/${NAME}" "/usr/bin/${NAME}"
EOT
chmod 0775 "${WORKDIR}/DEBIAN/postinst"

# postrm file
cat << EOT > "${WORKDIR}/DEBIAN/postrm"
rm "/usr/bin/${NAME}"
EOT
chmod 0775 "${WORKDIR}/DEBIAN/postrm"

dpkg-deb --build --root-owner-group "${WORKDIR}"
rm -rf "${WORKDIR}"

############################################################
# Little Navconnect ########################################
############################################################

# release build of little navconnect
DEPLOY_NAME="Little Navconnect"
DEPLOY_DIR="${APROJECTS}/deploy/${DEPLOY_NAME}"

# package information
NAME="littlenavconnect"
VERSION=$(head -n1 "${DEPLOY_DIR}/version.txt")
REVISION="1"
ARCH="amd64"

# setup workdir
WORKDIR="${APROJECTS}/deb/${DISTRIB_ID}_${DISTRIB_RELEASE}/${NAME}_${VERSION}-${REVISION}_${ARCH}"
rm -rf "${WORKDIR}" "${WORKDIR}.deb"
mkdir -p "${WORKDIR}"

# copy release
mkdir "${WORKDIR}/opt"
cp -ra "${DEPLOY_DIR}" "${WORKDIR}/opt"

# create meta information
mkdir "${WORKDIR}/DEBIAN"
cat << EOT > "${WORKDIR}/DEBIAN/control"
Package: ${NAME}
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: Daniel Buschke <damage@devloop.de>
Description: Little Navconnect
 Little Navconnect is a free open source application that acts as an agent connecting Little Navmap
 with a FSX, Prepar3D, Microsoft Flight Simulator 2020 or X-Plane flight simulator.
Homepage: https://albar965.github.io
EOT

# add desktop file to global application directory
mkdir -p "${WORKDIR}/usr/share/applications"
cp "${WORKDIR}/opt/${DEPLOY_NAME}/${DEPLOY_NAME}.desktop" "${WORKDIR}/usr/share/applications/${NAME}.desktop"
sed -i "s;YOUR_PATH/Little Navmap/Little Navconnect;/opt/${DEPLOY_NAME};g" "${WORKDIR}/usr/share/applications/${NAME}.desktop"
sed -i "s;YOUR_PATH/Little\\\\sNavmap/Little\\\\sNavconnect;/opt/${DEPLOY_NAME};g" "${WORKDIR}/usr/share/applications/${NAME}.desktop"

# postinst file
cat << EOT > "${WORKDIR}/DEBIAN/postinst"
ln -s "/opt/${DEPLOY_NAME}/${NAME}" "/usr/bin/${NAME}"
EOT
chmod 0775 "${WORKDIR}/DEBIAN/postinst"

# postrm file
cat << EOT > "${WORKDIR}/DEBIAN/postrm"
rm "/usr/bin/${NAME}"
EOT
chmod 0775 "${WORKDIR}/DEBIAN/postrm"

dpkg-deb --build --root-owner-group "${WORKDIR}"
rm -rf "${WORKDIR}"

