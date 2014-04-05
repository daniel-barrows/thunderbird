/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
// vim:set ts=2 sw=2 sts=2 et ft=javascript:

Components.utils.import("resource:///modules/Services.jsm");

/**
 * This file exports the JSMime code, polyfilling code as appropriate for use in
 * Gecko.
 */

// Load the core MIME parser. Since it doesn't define EXPORTED_SYMBOLS, we must
// use the subscript loader instead.
Services.scriptloader.loadSubScript("resource:///modules/jsmime/jsmime.js");

var EXPORTED_SYMBOLS = ["jsmime"];


// A polyfill to support non-encoding-spec charsets. Since the only converter
// available to us from JavaScript has a very, very weak and inflexible API, we
// choose to rely on the regular text decoder unless absolutely necessary.
// support non-encoding-spec charsets.
function FakeTextDecoder(label="UTF-8", options = {}) {
  this._reset(label);
  // So nsIScriptableUnicodeConverter only gives us fatal=false, unless we are
  // using UTF-8, where we only get fatal=true. The internals of said class tell
  // us to use a C++-only class if we need better behavior.
}
FakeTextDecoder.prototype = {
  _reset: function (label) {
    this._encoder = Components.classes[
      "@mozilla.org/intl/scriptableunicodeconverter"]
      .createInstance(Components.interfaces.nsIScriptableUnicodeConverter);
    this._encoder.isInternal = true;
    this._encoder.charset = label;
  },
  get encoding() { return this._encoder.charset; },
  decode: function (input, options = {}) {
    let more = 'stream' in options ? options.stream : false;
    let result = "";
    if (input !== undefined) {
      let data = new Uint8Array(input);
      result = this._encoder.convertFromByteArray(data, data.length);
    }
    // This isn't quite right--it won't handle errors if there are a few
    // remaining bytes in the buffer, but it's the best we can do.
    if (!more)
      this._reset(this.encoding);
    return result;
  },
};

let RealTextDecoder = TextDecoder;
function FallbackTextDecoder(charset, options) {
  try {
    return new RealTextDecoder(charset, options);
  } catch (e) {
    return new FakeTextDecoder(charset, options);
  }
}

TextDecoder = FallbackTextDecoder;
