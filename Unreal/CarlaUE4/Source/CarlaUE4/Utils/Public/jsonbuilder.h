/**
 * @file jsonbuilder.hpp
 * a simple and fast JSon generator in plain C/C++ with no dependency.
 *
 *
 * @author amir zamani
 * @version 1.0.0
 * @date 2014-05-14
 *
 * @author amir zamani
 * @version 1.0.1
 * @date 2014-05-16
 * new method : addNull and addValue (unnamed)
 *
 */
#ifndef _JSONBUILDER_HPP__
#define _JSONBUILDER_HPP__

#include <string.h>

///////////////////////////////////////////////////////////////////////////////
namespace gason {
///////////////////////////////////////////////////////////////////////////////
/** a string builder class.
 * mainly used as a base for JSonBuilder. this class does not own the buffer.
 */
class StringBuilder
{
public:
    /** default ctor
     * @param buffer result buffer to hold builded string.
     * @param length length (capacity of buffer).
     */
    explicit StringBuilder(char* buffer, size_t length)
        : ibuffer(buffer), ilength(length), iindex(0) {
        reset();
    }

    /** resets the string object. (writes NULL into buffer). */
    void     reset() {
        memset(ibuffer, 0, ilength);
        iindex = 0;
    }

    /** returns the empty spaces left. return 0 if the buffer is full. */
    size_t   emptySpaces() const {
        return ( iindex < ilength ) ? (ilength - 1 - iindex) : 0;
    }

    StringBuilder&  append(const char* string, size_t length=0) {
        if ( length == 0 )
            length = strlen(string);

        if ( ( length + iindex ) < ilength ) {
            strncat(ibuffer+iindex, string, length);
            iindex += length;
        }
        return *this;
    }

    StringBuilder&  operator << (const char* string) {
        return append(string);
    }

    StringBuilder&  operator << (int number) {
        int length = snprintf(ibuffer+iindex,
                              ilength-iindex,
                              "%d", number);
        if ( length > 0 )
            iindex += length;

        return *this;
    }

    StringBuilder&  operator << (double number) {
        int length = snprintf(ibuffer+iindex,
                              ilength-iindex,
                              "%lf", number);
        if ( length > 0 )
            iindex += length;

        return *this;
    }

#if defined(QBYTEARRAY_H)
    StringBuilder&  operator << (const QByteArray& bytes) {
        return append(bytes.constData());
    }
#endif

#if defined(QSTRING_H)
    StringBuilder&  operator << (const QString& string) {
        return append(string.toUtf8().constData());
    }
#endif

protected:
    char*       ibuffer;        ///< internal pointer to the start of the buffer.
    size_t      ilength;        ///< length (capacity) of buffer.
    size_t      iindex;         ///< current writing index.
};
///////////////////////////////////////////////////////////////////////////////
/** JSonBuilder. uses @sa StringBuilder as backend.
 */
class JSonBuilder : protected StringBuilder
{
public:
    /** default ctor
     * @param buffer result buffer to hold builded string.
     * @param length length (capacity of buffer).
     */
    explicit JSonBuilder(char *buffer, size_t length) : StringBuilder(buffer, length) {
    }

    using StringBuilder::reset;
    using StringBuilder::emptySpaces;

    /** checks if the specified buffer is not small and holds the proper result json string. */
    bool            isBufferAdequate() const {
        return emptySpaces() > 0;
    }

    /** starts an object by @code { @endcode or @code "name" : { @endcode if name is not empty. */
    JSonBuilder&    startObject(const char* name = 0) {
        addPossibleComma();
        if ( name != 0    &&    strlen(name) > 0 ) {
            *this << "\"" << name << "\":";
        }

        operator <<("{");
        return *this;
    }

    /** ends an object by @code } @endcode. */
    JSonBuilder&    endObject() {
        operator <<("}");
        return *this;
    }

    /** ends an object by @code , @endcode. */
    JSonBuilder&    comma() {
        operator <<(",");
        return *this;
    }

    /** starts an array by @code [ @endcode or @code "name" : [ @endcode if name is not empty. */
    JSonBuilder&    startArray(const char* name = 0) {
        addPossibleComma();
        if ( name != 0    &&    strlen(name) > 0 ) {
            *this << "\"" << name << "\":";
        }

        operator <<("[");
        return *this;
    }

    /** ends an array by @code ] @endcode. */
    JSonBuilder&    endArray() {
        operator <<("]");
        return *this;
    }

    /** writes a value by @code "name" : "value" @endcode. */
    JSonBuilder&    addValue(const char* name, const char* value) {
        addPossibleComma() << "\"" << name << "\":\"" << value << "\"";
        return *this;
    }

    /** writes a value by @code "name" : number @endcode. */
    JSonBuilder&    addValue(const char *name, int number) {
        addPossibleComma() << "\"" << name << "\":" << number;
        return *this;
    }

    /** writes a value by @code "name" : number @endcode. */
    JSonBuilder&    addValue(const char *name, size_t number) {
        addPossibleComma() << "\"" << name << "\":" << (int) number;
        return *this;
    }

    /** writes a value by @code "name" : number @endcode. */
    JSonBuilder&    addValue(const char *name, double number) {
        addPossibleComma() << "\"" << name << "\":" << number;
        return *this;
    }

    /** writes a value by @code "name" : true/false @endcode. */
    JSonBuilder&    addValue(const char *name, bool state) {
        addPossibleComma() << "\"" << name << "\":" << ((state) ? "true":"false");
        return *this;
    }

    /** writes a null value by @code "name" : null @endcode. */
    JSonBuilder&    addNull(const char *name) {
        addPossibleComma() << "\"" << name << "\":" << "null";
        return *this;
    }

    // for unnamed elements
    /** writes an unnamed value by @code "value" @endcode. */
    JSonBuilder&    addValue(const char* value) {
        addPossibleComma()  << "\"" << value << "\"";
        return *this;
    }

    /** writes a unnamed value by @code number @endcode. */
    JSonBuilder&    addValue(int number) {
        addPossibleComma() << number;
        return *this;
    }

    /** writes a unnamed value by @code number @endcode. */
    JSonBuilder&    addValue(size_t number) {
        addPossibleComma() << (int) number;
        return *this;
    }

    /** writes a unnamed value by @code number @endcode. */
    JSonBuilder&    addValue(double number) {
        addPossibleComma() << number;
        return *this;
    }

    /** writes a unnamed value by @code true/false @endcode. */
    JSonBuilder&    addValue(bool state) {
        addPossibleComma() << ((state) ? "true":"false");
        return *this;
    }

    /** writes a null value by @code null @endcode. */
    JSonBuilder&    addNull() {
        addPossibleComma() << "null";
        return *this;
    }


#if defined(QBYTEARRAY_H)
    /** writes a value by @code "name" : "value" @endcode. */
    JSonBuilder&    addValue(const char *name, const QByteArray& value) {
        addPossibleComma() << "\"" << name << "\":\"" << value << "\"";
        return *this;
    }
#endif

#if defined(QSTRING_H)
    /** writes a value by @code "name" : "value" @endcode. */
    JSonBuilder&    addValue(const char *name, const QString& value) {
        addPossibleComma() << "\"" << name << "\":\"" << value << "\"";
        return *this;
    }
#endif

protected:
    JSonBuilder&    addPossibleComma() {
        if ( iindex > 0 ) {
            char prev = ibuffer[iindex-1];
            if ( prev != '{'    &&    prev != '['    &&    prev != ',' ) {
                *this << ",";
            }
        }

        return *this;
    }

};



///////////////////////////////////////////////////////////////////////////////
} // namespace gason
///////////////////////////////////////////////////////////////////////////////
#endif // _JSONBUILDER_HPP__
