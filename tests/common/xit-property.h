/*
 * Copyright © 2012-2013 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif
#ifndef _ATOM_HELPERS_H_
#define _ATOM_HELPERS_H_

#include <stdint.h>
#include <xorg/gtest/xorg-gtest.h>
#include <X11/Xatom.h>

#define ASSERT_PROPERTY(_type, _name, _dpy, _deviceid, _propname) \
    ASSERT_TRUE(DevicePropertyExists(_dpy, _deviceid, _propname)); \
    XITProperty<_type> _name(_dpy, _deviceid, _propname);


template <typename DataType>
class XITProperty {
private:
    /* I'm too lazy for that right now. */
    XITProperty(const XITProperty&);
    XITProperty& operator=(const XITProperty &);

    ::Display *dpy;
    int deviceid;

public:
    Atom type;
    int format;
    unsigned long nitems;

    DataType *data;

    Atom prop;
    std::string propname;

    XITProperty(::Display *dpy, int deviceid, const std::string &propname);
    XITProperty(::Display *dpy, int deviceid, const std::string &propname,
                Atom type, int format, unsigned long nitems, const DataType *data);
    ~XITProperty();

    /**
     * Resize the data member to hold nitems.
     */
    void Resize(size_t nitems);
    void Update(void);

    /**
     * Get value at index, throw an exception if we're OOB.
     */
    DataType At(size_t index) const;

    /**
     * Set value at index, resizing data as-needed.
     */
    void Set(size_t index, DataType value);
};


bool DevicePropertyExists(::Display *dpy, int deviceid, const std::string &propname) {
    Atom prop;

    prop = XInternAtom(dpy, propname.c_str(), True);
    if (prop == None)
        return false;

    Atom *props;
    int nprops;
    props = XIListProperties(dpy, deviceid, &nprops);

    bool prop_found = false;
    while (nprops-- && !prop_found)
        prop_found = (props[nprops] == prop);

    XFree(props);

    return prop_found;
}

template <typename DataType>
XITProperty<DataType>::XITProperty(::Display *dpy, int deviceid, const std::string &propname)
{
    this->dpy = dpy;
    this->deviceid = deviceid;
    this->format = -1;
    this->data = NULL;

    if (!DevicePropertyExists(dpy, deviceid, propname)) {
        ADD_FAILURE() << "Property " << propname << " does not exist on device";
        return;
    }

    prop = XInternAtom(dpy, propname.c_str(), True);

    unsigned char *d;

    unsigned long bytes_after;
    XIGetProperty(dpy, deviceid, prop, 0, 1000, False,
                  AnyPropertyType, &type, &format, &nitems, &bytes_after, &d);

    /* XI2 32-bit properties are actually 32 bit, Atom is > 4, so copy over
     * where needed */
    if (d && sizeof(DataType) > 4) {
        data = new DataType[nitems];
        for (unsigned int i = 0; i < nitems; i++)
            data[i] = reinterpret_cast<int32_t*>(d)[i];
        XFree(d);
    } else
        data = reinterpret_cast<DataType*>(d);
}

template <typename DataType>
XITProperty<DataType>::XITProperty(::Display *dpy, int deviceid, const std::string &propname,
                                   Atom type, int format, unsigned long nitems, const DataType *data)
{
    if (format != 8 && format != 16 && format != 32) {
        ADD_FAILURE()  << "Invalid format " << format;
        return;
    }
    this->format = format;
    this->nitems = nitems;
    this->data = new DataType[nitems];

    memcpy(this->data, data, nitems * sizeof(data));

    union  {
        unsigned char *c;
        unsigned short *s;
        uint32_t *i;
    } d;

    d.c = new unsigned char[format/8 * nitems];

    for (unsigned int i = 0; i < nitems; i++) {
        switch(format) {
            case  8: d.c[i] = data[i]; break;
            case 16: d.s[i] = data[i]; break;
            case 32: d.i[i] = data[i]; break;
        }
    }

    prop = XInternAtom(dpy, propname.c_str(), False);
    XIChangeProperty(dpy, deviceid, prop, type, format, PropModeReplace, d.c, nitems);
    XSync(dpy, False);

    delete[] d.c;
}


template <typename DataType>
void XITProperty<DataType>::Update(void)
{
    unsigned char *d;

    /* XI2 32-bit properties are actually 32 bit, Atom is > 4, so copy over
     * where needed */
    if (sizeof(DataType) > 4) {
        int32_t tmp[nitems];
        for (unsigned int i = 0; i < nitems; i++)
            tmp[i] = data[i];
        d = reinterpret_cast<unsigned char*>(tmp);
    } else
        d = reinterpret_cast<unsigned char*>(data);

    XIChangeProperty(dpy, deviceid, prop, type, format, PropModeReplace, d, nitems);
    XSync(dpy, False);
}

template <typename DataType>
void XITProperty<DataType>::Resize(size_t nitems)
{
    size_t sz = (sizeof(DataType) >= 4) ? 4 : sizeof(DataType);

    data = reinterpret_cast<DataType*>(realloc(data, nitems * sz));

    if (nitems > this->nitems)
        memset(&data[this->nitems], 0, (nitems - this->nitems) * sz);

    this->nitems = nitems;
}

template <typename DataType>
XITProperty<DataType>::~XITProperty()
{
    XFree(data);
}

template <typename DataType>
DataType XITProperty<DataType>::At(size_t index) const
{
    if (index > nitems)
        throw new std::runtime_error("Array index out of bounds");
    return data[index];
}

template <typename DataType>
void XITProperty<DataType>::Set(size_t index, DataType value)
{
    if (index >= nitems)
        Resize(index + 1);
    data[index] = value;
}

#endif /* _ATOM_HELPERS_H_ */
