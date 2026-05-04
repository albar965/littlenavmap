#!/bin/bash

# Echo all commands and exit on failure
set -euxo pipefail

bash pull_all.sh

bash build_release.sh

bash create_deb.sh

bash pack_deploy.sh


