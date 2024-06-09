#!/bin/bash

set -e
set -x

WORKDIR=`dirname ${0}`

bash "${WORKDIR}/../build_release.sh"

bash "${WORKDIR}/../create_deb.sh"

bash "${WORKDIR}/../pack_deploy.sh"

