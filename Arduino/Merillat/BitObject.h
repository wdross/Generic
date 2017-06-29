#ifndef BITOBJECT_H
#define BITOBJECT_H

class BitObject
{
public:
  inline BitObject(INT8U *address, INT8U nbit);
  inline bool Read();

private:
  INT8U *Address;
  INT8U Bit;
};

inline BitObject::BitObject(INT8U *address, INT8U nbit) {
  // save access information locally
  Address = address;
  Bit = nbit;
}
inline bool BitObject::Read() {
  return((*Address >> Bit) & 0x1);
}
#endif // BITOBJECT_H
