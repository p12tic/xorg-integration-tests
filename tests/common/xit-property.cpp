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

#include <xit-property.h>

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


