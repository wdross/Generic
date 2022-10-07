#!/bin/bash

# In order to reset the AT&T home base:
rm 192.168.0.1 -r
wget http://192.168.0.1/index.html -r -q

# "log in"
# POST isTest=false&goformId=LOGIN&password=YWRtaW4=
wget --post-data "isTest=false&action=clear&goformId=FOTA_SETTINGS" http://192.168.0.1/goform/goform_set_cmd_process -O fota
wget --post-data "isTest=false&goformId=LOGIN&password=YWRtaW4=" http://192.168.0.1/goform/goform_set_cmd_process -O login

# go to http://192.168.0.1/index.html#restore

# submit class="btn-1 " type="submit" data-trans="restart" and value="Restart"
# POST isTest=false&goformId=REBOOT_DEVICE
wget --post-data "isTest=false&goformId=REBOOT_DEVICE" http://192.168.0.1/goform/goform_set_cmd_process -O reboot

# calling script needs to make sure to not call this too often
