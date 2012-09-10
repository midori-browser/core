/*
 Copyright (C) 2012 Christian Dywan <christian@twotoasts.de>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 See the file COPYING for the full license text.
*/

static void http_hsts () {
    Midori.HSTS.Directive d;
    d = new Midori.HSTS.Directive.from_header ("max-age=31536000");
    assert (d.is_valid () && !d.sub_domains);
    d = new Midori.HSTS.Directive.from_header ("max-age=15768000 ; includeSubDomains");
    assert (d.is_valid () && d.sub_domains);

    /* Invalid */
    d = new Midori.HSTS.Directive.from_header ("");
    assert (!d.is_valid () && !d.sub_domains);
    d = new Midori.HSTS.Directive.from_header ("includeSubDomains");
    assert (!d.is_valid () && d.sub_domains);
}

void main (string[] args) {
    Test.init (ref args);
    Test.add_func ("/http/hsts", http_hsts);
    Test.run ();
}

