#!/bin/bash
# extract database parameters from slowcontrol config file
. <(grep dataBaseString ~/.slowcontrol | tr '" ' "\n\n" | grep .\\+=.\\+)
pg_dump -h ${host} -s ${dbname} | grep -v ${user} > database.sql 
