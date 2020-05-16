# Parse messages of the form:
# 0    19FF0100         8  C0  10  32  99  00  F1  00  00       0.473
# 0    19FF0100         8  C1  3B  1B  00  F9  04  00  00       0.473
# 0    19FF0100         8  C2  00  00  00  FF  FF  FF  FF       0.474
#$1         $2          $3 $4  $5  $6  $7  $8  $9 $10 $11       $12

function Hex2Dec(value ) {
  return strtonum("0x"value)
}

BEGIN {
  if (North)
    inst += 1
  if (South)
    inst += 2
  # else inst remains 0, do nothing
  Door[1] = "North"
  Door[2] = "South"
  LatchFC = -1
  printf(" Time(s), Door,MTR(C),PPC(C),Amps,  %%\n")
  printf(" -------, ----,------,------,----,---\n")
}

/19FF0100/ {
  # find the status COBID coming from the gateway
  # the first byte's least 3 bits are Sequence Number
  B1 = Hex2Dec($4)
  SN = B1 % 32
  FC = int(B1 / 32) # Frame Counter
  if (SN == 0) {
    NDB = Hex2Dec($5)
    B5 = Hex2Dec($9)
    Instance = B5 % 16
    if (NDB == 16 && and(Instance,inst)) {
      LatchFC = FC
      Time = $12
      LastSN = 0
    }
    else  {
      if (NDB == 6 && and(Instance,inst))
        printf("%8s,%5s,%s\n",Time,Door[Instance],substr($0,35,22))
      LatchFC = -1
    }
  }
  else if (FC == LatchFC && SN == 1 && LastSN == 0) {
    MotorTemp = Hex2Dec($5)
    InverterTemp = Hex2Dec($6)
    LastSN = SN
  }
  else if (FC == LatchFC && SN == 2 && LastSN == 1) {
    Amps = Hex2Dec($6 $5)
    Thrust = Hex2Dec($7)
#    printf("%8s,%5s,%6d,%6d,%4d,%3d\n",Time,Door[Instance],MotorTemp,InverterTemp,Amps/10,Thrust)
  }
}