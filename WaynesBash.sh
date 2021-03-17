#!/bin/bash
#

# See if we are running in home, and if so, let's make it so we auto-run
# with each login -- the whole idea of having a customized script, right?!?
if [ $(pwd) == $HOME ]; then
  grep $0 .bashrc &>/dev/null || { sed -i -e "\$a#\n# Include my favorites:\n$0" .bashrc; }

#  if [ $(grep -c history-search- .inputrc) -lt 2 ]; then
#    sed -i '$ a "\e[A": history-search-backward"
#    sed -i '$ a "\e[B": history-search-forward"
#  fi
fi

# Get time in seconds:
S=$(awk "{print int(\$1)}" /proc/uptime )
# if bc is not available, install it:
type -P bc &>/dev/null || { echo "Let's install bc"; sudo apt-get install bc; }

# check again to see if user turned us down, we can't use it
if [ $S -lt 360 -o X$(type -P bc) == X ]; then
  echo "On for "$S" seconds"
else
  S=$(echo "scale=2; "$S" / 60" | bc)
  intS=$(echo "( $S + 0.5 ) / 1" | bc)

  if [ $intS -lt 120 ]; then
    echo "On for "$S" minutes"
  else
    S=$(echo "scale=2; "$S" / 60" | bc)
    intS=$(echo "( $S + 0.5 ) / 1" | bc)
    if [ $intS -lt 48 ]; then
      echo "On for "$S" hours"
    else
      S=$(echo "scale=2; "$S" / 24" | bc)
      intS=$(echo "( $S + 0.5 ) / 1" | bc)
      if [ $intS -lt 28 ]; then
        echo "On for "$S" days"
      else
        S=$(echo "scale=2; "$S" / 7" | bc)
        intS=$(echo "( $S + 0.5 ) / 1" | bc)
        if [ $intS -lt 104 ]; then
          echo "On for "$S" weeks"
        else
          S=$(echo "scale=2; "$S" / 52" | bc)
          echo "On for "$S" years"
        fi
      fi
    fi
  fi
fi
echo ""
df -h
