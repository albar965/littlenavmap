#!/bin/bash

set -e
set -x

bash pull_all.sh

bash build_release.sh

bash create_deb.sh

bash pack_deploy.sh


