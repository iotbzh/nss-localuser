#!/bin/sh

action=$(echo ${1:-status} | tr '[a-z]' '[A-Z]')
file=${2:-/etc/nsswitch.conf}

# compute activated value
activated=false
if grep -q '^hosts:.*\<localuser\>' "${file}" 2>/dev/null; then
  activated=true
fi

# compute activate value
case ${action} in
  ON|YES|TRUE|1) activate=true;;
  OFF|NO|FALSE|0) activate=false;;
  QUERY|TEST|CHECK|STATUS) ${activated} && echo ON || echo OFF; exit 0;;
  *) echo "Bad command: $1" >&2; exit 1;;
esac

# exit if already set as desired
[ "${activate}" = "${activated}" ] && exit 0

# process
if ${activate}; then
  sedcmd='/^hosts:/s/hosts:[ \t]*/&localuser /'
else
  sedcmd='/^hosts:/s/localuser *//'
fi
if ! cp "${file}" "${file}~"; then
  echo "Can't save file ${file} to ${file}~" >&2
  exit 1
fi
if ! grep -q '^hosts:' "${file}"; then
  if ! echo >> "${file}" || ! echo "hosts:	" >> "${file}"; then
    echo "Can't add host: to ${file}"  >&2
    cp "${file}~" "${file}" && rm "${file}~"
    exit 1
  fi
fi
if ! sed -i "${sedcmd}" "${file}"; then
  echo "Can't process ${file}"  >&2
  cp "${file}~" "${file}" && rm "${file}~"
  exit 1
fi
