#!/bin/bash

# Echo all commands and exit on failure
set -e
set -x

# Error checking for required variable APROJECTS
if [ -z "${APROJECTS}" ] ; then echo APROJECTS environment variable not set ; exit 1 ; fi
if [ ! -d "${APROJECTS}" ]; then echo "${APROJECTS}" does not exist ; exit 1 ; fi

# load system we are running on
source /etc/os-release

# define release build names and directories
# little navmap
DEPLOY_NAVMAP_NAME="Little Navmap"
DEPLOY_NAVMAP_DIR="${APROJECTS}/deploy/${DEPLOY_NAVMAP_NAME}"
PKG_NAME_NAVMAP="littlenavmap"

# little navconnect
DEPLOY_NAVCON_NAME="Little Navconnect"
DEPLOY_NAVCON_DIR="${APROJECTS}/deploy/${DEPLOY_NAVCON_NAME}"
PKG_NAME_NAVCON="littlenavconnect"

# little xpconnect
DEPLOY_XPCON_NAME="Little Xpconnect"
DEPLOY_XPCON_DIR="${APROJECTS}/deploy/${DEPLOY_XPCON_NAME}"

############################################################
# Little Navmap ############################################
############################################################

# package information
VERSION=$(head -n1 "${DEPLOY_NAVMAP_DIR}/version.txt")
REVISION="1"
ARCH="amd64"

# clean setup workdir
WORKDIR="${APROJECTS}/deploy/${ID}_${VERSION_ID}/${PKG_NAME_NAVMAP}_${VERSION}-${REVISION}_${ARCH}"
rm -rf "${WORKDIR}" "${WORKDIR}.deb"
mkdir -p "${WORKDIR}"

# copy navmap release
mkdir "${WORKDIR}/opt"
cp -ra "${DEPLOY_NAVMAP_DIR}" "${WORKDIR}/opt/"

# copy navcon release
cp -ra "${DEPLOY_NAVCON_DIR}" "${WORKDIR}/opt/${DEPLOY_NAVMAP_NAME}/"

# copy xpcon release
cp -ra "${DEPLOY_XPCON_DIR}" "${WORKDIR}/opt/${DEPLOY_NAVMAP_NAME}/"

# create meta information
mkdir "${WORKDIR}/DEBIAN"
cat << EOT > "${WORKDIR}/DEBIAN/control"
Package: ${PKG_NAME_NAVMAP}
Version: ${VERSION}
Architecture: ${ARCH}
Maintainer: Daniel Buschke <damage@devloop.de>
Description: Little Navmap
 Little Navmap is a free open source flight planner, navigation tool, moving map, airport search and airport information system.
 This package includes Little Navconnect and Little Xpconnect
Homepage: https://albar965.github.io
EOT

# add navmap desktop file to global application directory
mkdir -p "${WORKDIR}/usr/share/applications"
cp "${WORKDIR}/opt/${DEPLOY_NAVMAP_NAME}/${DEPLOY_NAVMAP_NAME}.desktop" "${WORKDIR}/usr/share/applications/${PKG_NAME_NAVMAP}.desktop"
sed -i "s;YOUR_PATH_TO_LITTLENAVMAP;/opt/${DEPLOY_NAVMAP_NAME};g" "${WORKDIR}/usr/share/applications/${PKG_NAME_NAVMAP}.desktop"

# add navcon desktop file to global application directory
mkdir -p "${WORKDIR}/usr/share/applications"
cp "${WORKDIR}/opt/${DEPLOY_NAVMAP_NAME}/${DEPLOY_NAVCON_NAME}/${DEPLOY_NAVCON_NAME}.desktop" "${WORKDIR}/usr/share/applications/${PKG_NAME_NAVCON}.desktop"
sed -i "s;YOUR_PATH/Little Navmap/Little Navconnect;/opt/${DEPLOY_NAVMAP_NAME}/${DEPLOY_NAVCON_NAME};g" "${WORKDIR}/usr/share/applications/${PKG_NAME_NAVCON}.desktop"
sed -i "s;YOUR_PATH/Little\\\\sNavmap/Little\\\\sNavconnect;/opt/${DEPLOY_NAVMAP_NAME}/${DEPLOY_NAVCON_NAME};g" "${WORKDIR}/usr/share/applications/${PKG_NAME_NAVCON}.desktop"

# postinst file
cat << EOT > "${WORKDIR}/DEBIAN/postinst"
ln -s "/opt/${DEPLOY_NAVMAP_NAME}/${PKG_NAME_NAVMAP}" "/usr/bin/${PKG_NAME_NAVMAP}"
ln -s "/opt/${DEPLOY_NAVMAP_NAME}/${DEPLOY_NAVCON_NAME}/${PKG_NAME_NAVCON}" "/usr/bin/${PKG_NAME_NAVCON}"
EOT
chmod 0775 "${WORKDIR}/DEBIAN/postinst"

# postrm file
cat << EOT > "${WORKDIR}/DEBIAN/postrm"
rm -f "/usr/bin/${PKG_NAME_NAVMAP}"
rm -f "/usr/bin/${PKG_NAME_NAVCON}"
EOT
chmod 0775 "${WORKDIR}/DEBIAN/postrm"

dpkg-deb --build --root-owner-group "${WORKDIR}"

# No compression for debugging dpkg-deb -Znone --build --root-owner-group "${WORKDIR}"

FILENAME_LNM=$VERSION_ID-$(head -n1 "${APROJECTS}/deploy/Little Navmap/version.txt")

# Rename file to match other LNM archive names
cp -av "${WORKDIR}".deb ${APROJECTS}/deploy/LittleNavmap-linux-${FILENAME_LNM}-${REVISION}_${ARCH}.deb

#rm -rf "${APROJECTS}/deploy/${ID}_${VERSION_ID}"

