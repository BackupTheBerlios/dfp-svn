#!/bin/sh

#scp website/* mmolteni@shell.berlios.de:/home/groups/dfp/htdocs/
rsync --delete -vaz website/* mmolteni@shell.berlios.de:/home/groups/dfp/htdocs/
