#ifndef __STRUCTIO_H_
#define __STRUCTIO_H_

template <typename T>
inline void writeStruct(Stream &stream, const T *header)
{
	writeStruct(stream, header, sizeof(T));
}

template <typename T>
inline void writeStruct(Stream &stream, const T *header, size_t length)
{
	stream.write((const uint8_t *)header, length);
}

template <typename T>
inline bool readStruct(Stream &stream, T *header)
{
	return readStruct(stream, header, sizeof(T));
}

template <typename T>
inline bool readStruct(Stream &stream, T *header, size_t length)
{
	return (stream.readBytes((uint8_t *)header, sizeof(T)) == sizeof(T));
}

#endif // __STRUCTIO_H_