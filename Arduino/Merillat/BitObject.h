#ifndef BITOBJECT_H
#define BITOBJECT_H

class BitObject
{
public:
  // Address of byte, Position of Least Significant Bit, Field size (default 1)
  inline BitObject(INT8U *address, INT8U lsbit, INT8U fsize=1);
  inline INT8U Read();
  inline void Write(INT8U value);

private:
  INT8U *Address;
  INT8U Bit;
  INT8U Mask;
};

inline BitObject::BitObject(INT8U *address, INT8U lsbit, INT8U fsize) {
  // save access information locally
  Address = address;
  Bit = lsbit;
  Mask = (0x1 << fsize) - 1;
}
inline INT8U BitObject::Read() {
  return((*Address >> Bit) & Mask);
}
inline void BitObject::Write(INT8U value)
{
  if (Address) {
    *Address = (*Address & ~(Mask<<Bit)) | ((value&Mask) << Bit);
  }
}
#endif // BITOBJECT_H
