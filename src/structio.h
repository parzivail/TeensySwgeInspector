#ifndef __STRUCTIO_H_
#define __STRUCTIO_H_

template <typename T>
inline void writeStruct(Stream &radio, const T *header)
{
	writeStruct(radio, header, sizeof(T));
}

template <typename T>
inline void writeStruct(Stream &radio, const T *header, size_t length)
{
	radio.write((const char *)header, length);
}

template <typename T>
inline bool readStruct(Stream &radio, T *header)
{
	while (radio.available() < sizeof(T))
		;
	return (radio.readBytes((char *)header, sizeof(T)) == sizeof(T));
}

#endif // __STRUCTIO_H_