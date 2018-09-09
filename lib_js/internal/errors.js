/* eslint node-core/documented-errors: "error" */
/* eslint node-core/alphabetize-errors: "error" */
/* eslint node-core/prefer-util-format-errors: "error" */

'use strict';

// The whole point behind this internal module is to allow Node.js to no
// longer be forced to treat every error message change as a semver-major
// change. The NodeError classes here all expose a `code` property whose
// value statically and permanently identifies the error. While the error
// message may change, the code should not.

const codes = {};

// Utility function for registering the error codes. Only used here. Exported
// *only* to allow for testing.
function E(sym, val, def, ...otherClasses) {
    codes[sym] = def;
}

module.exports = exports = {
    codes
};

// To declare an error message, use the E(sym, val, def) function above. The sym
// must be an upper case string. The val can be either a function or a string.
// The def must be an error class.
// The return value of the function must be a string.
// Examples:
// E('EXAMPLE_KEY1', 'This is the error value', Error);
// E('EXAMPLE_KEY2', (a, b) => return `${a} ${b}`, RangeError);
//
// Once an error code has been assigned, the code itself MUST NOT change and
// any given error code must never be reused to identify a different error.
//
// Any error code added here should also be added to the documentation
//
// Note: Please try to keep these in alphabetical order
//
// Note: Node.js specific errors must begin with the prefix ERR_

E('ERR_AMBIGUOUS_ARGUMENT', 'The "%s" argument is ambiguous. %s', TypeError);
E('ERR_ARG_NOT_ITERABLE', '%s must be iterable', TypeError);
E('ERR_ASSERTION', '%s', Error);
E('ERR_ASYNC_CALLBACK', '%s must be a function', TypeError);
E('ERR_ASYNC_TYPE', 'Invalid name for async "type": %s', TypeError);
E('ERR_BUFFER_OUT_OF_BOUNDS', 'Buffer is out of bounds', RangeError);
E('ERR_BUFFER_TOO_LARGE',
    `Cannot create a Buffer this large`,
    RangeError);
E('ERR_CANNOT_WATCH_SIGINT', 'Cannot watch for SIGINT signals', Error);
E('ERR_CHILD_CLOSED_BEFORE_REPLY',
    'Child closed before reply received', Error);
E('ERR_CHILD_PROCESS_IPC_REQUIRED',
    "Forked processes must have an IPC channel, missing value 'ipc' in %s",
    Error);
E('ERR_CHILD_PROCESS_STDIO_MAXBUFFER', '%s maxBuffer length exceeded',
    RangeError);
E('ERR_CONSOLE_WRITABLE_STREAM',
    'Console expects a writable stream instance for %s', TypeError);
E('ERR_CPU_USAGE', 'Unable to obtain cpu usage %s', Error);
E('ERR_CRYPTO_CUSTOM_ENGINE_NOT_SUPPORTED',
    'Custom engines not supported by this OpenSSL', Error);
E('ERR_CRYPTO_ECDH_INVALID_FORMAT', 'Invalid ECDH format: %s', TypeError);
E('ERR_CRYPTO_ECDH_INVALID_PUBLIC_KEY',
    'Public key is not valid for specified curve', Error);
E('ERR_CRYPTO_ENGINE_UNKNOWN', 'Engine "%s" was not found', Error);
E('ERR_CRYPTO_FIPS_FORCED',
    'Cannot set FIPS mode, it was forced with --force-fips at startup.', Error);
E('ERR_CRYPTO_FIPS_UNAVAILABLE', 'Cannot set FIPS mode in a non-FIPS build.',
    Error);
E('ERR_CRYPTO_HASH_DIGEST_NO_UTF16', 'hash.digest() does not support UTF-16',
    Error);
E('ERR_CRYPTO_HASH_FINALIZED', 'Digest already called', Error);
E('ERR_CRYPTO_HASH_UPDATE_FAILED', 'Hash update failed', Error);
E('ERR_CRYPTO_INVALID_DIGEST', 'Invalid digest: %s', TypeError);
E('ERR_CRYPTO_INVALID_STATE', 'Invalid state for operation %s', Error);

// Switch to TypeError. The current implementation does not seem right.
E('ERR_CRYPTO_SIGN_KEY_REQUIRED', 'No key provided to sign', Error);
E('ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH',
    'Input buffers must have the same length', RangeError);
E('ERR_DNS_SET_SERVERS_FAILED', 'c-ares failed to set servers: "%s" [%s]',
    Error);
E('ERR_DOMAIN_CALLBACK_NOT_AVAILABLE',
    'A callback was registered through ' +
    'process.setUncaughtExceptionCaptureCallback(), which is mutually ' +
    'exclusive with using the `domain` module',
    Error);
E('ERR_DOMAIN_CANNOT_SET_UNCAUGHT_EXCEPTION_CAPTURE',
    'The `domain` module is in use, which is mutually exclusive with calling ' +
    'process.setUncaughtExceptionCaptureCallback()',
    Error);
E('ERR_ENCODING_INVALID_ENCODED_DATA',
    'The encoded data was not valid for encoding %s', TypeError);
E('ERR_ENCODING_NOT_SUPPORTED', 'The "%s" encoding is not supported',
    RangeError);
E('ERR_FALSY_VALUE_REJECTION', 'Promise was rejected with falsy value', Error);
E('ERR_FS_FILE_TOO_LARGE', 'File size (%s) is greater than possible Buffer size',
    RangeError);
E('ERR_FS_INVALID_SYMLINK_TYPE',
    'Symlink type must be one of "dir", "file", or "junction". Received "%s"',
    Error); // Switch to TypeError. The current implementation does not seem right
E('ERR_HTTP2_ALTSVC_INVALID_ORIGIN',
    'HTTP/2 ALTSVC frames require a valid origin', TypeError);
E('ERR_HTTP2_ALTSVC_LENGTH',
    'HTTP/2 ALTSVC frames are limited to 16382 bytes', TypeError);
E('ERR_HTTP2_CONNECT_AUTHORITY',
    ':authority header is required for CONNECT requests', Error);
E('ERR_HTTP2_CONNECT_PATH',
    'The :path header is forbidden for CONNECT requests', Error);
E('ERR_HTTP2_CONNECT_SCHEME',
    'The :scheme header is forbidden for CONNECT requests', Error);
E('ERR_HTTP2_GOAWAY_SESSION',
    'New streams cannot be created after receiving a GOAWAY', Error);
E('ERR_HTTP2_HEADERS_AFTER_RESPOND',
    'Cannot specify additional headers after response initiated', Error);
E('ERR_HTTP2_HEADERS_SENT', 'Response has already been initiated.', Error);
E('ERR_HTTP2_HEADER_SINGLE_VALUE',
    'Header field "%s" must only have a single value', TypeError);
E('ERR_HTTP2_INFO_STATUS_NOT_ALLOWED',
    'Informational status codes cannot be used', RangeError);
E('ERR_HTTP2_INVALID_CONNECTION_HEADERS',
    'HTTP/1 Connection specific headers are forbidden: "%s"', TypeError);
E('ERR_HTTP2_INVALID_HEADER_VALUE',
    'Invalid value "%s" for header "%s"', TypeError);
E('ERR_HTTP2_INVALID_INFO_STATUS',
    'Invalid informational status code: %s', RangeError);
E('ERR_HTTP2_INVALID_PACKED_SETTINGS_LENGTH',
    'Packed settings length must be a multiple of six', RangeError);
E('ERR_HTTP2_INVALID_PSEUDOHEADER',
    '"%s" is an invalid pseudoheader or is used incorrectly', TypeError);
E('ERR_HTTP2_INVALID_SESSION', 'The session has been destroyed', Error);
E('ERR_HTTP2_INVALID_SETTING_VALUE',
    'Invalid value for setting "%s": %s', TypeError, RangeError);
E('ERR_HTTP2_INVALID_STREAM', 'The stream has been destroyed', Error);
E('ERR_HTTP2_MAX_PENDING_SETTINGS_ACK',
    'Maximum number of pending settings acknowledgements', Error);
E('ERR_HTTP2_NO_SOCKET_MANIPULATION',
    'HTTP/2 sockets should not be directly manipulated (e.g. read and written)',
    Error);
E('ERR_HTTP2_OUT_OF_STREAMS',
    'No stream ID is available because maximum stream ID has been reached',
    Error);
E('ERR_HTTP2_PAYLOAD_FORBIDDEN',
    'Responses with %s status must not have a payload', Error);
E('ERR_HTTP2_PING_CANCEL', 'HTTP2 ping cancelled', Error);
E('ERR_HTTP2_PING_LENGTH', 'HTTP2 ping payload must be 8 bytes', RangeError);
E('ERR_HTTP2_PSEUDOHEADER_NOT_ALLOWED',
    'Cannot set HTTP/2 pseudo-headers', TypeError);
E('ERR_HTTP2_PUSH_DISABLED', 'HTTP/2 client has disabled push streams', Error);
E('ERR_HTTP2_SEND_FILE', 'Directories cannot be sent', Error);
E('ERR_HTTP2_SEND_FILE_NOSEEK',
    'Offset or length can only be specified for regular files', Error);
E('ERR_HTTP2_SESSION_ERROR', 'Session closed with error code %s', Error);
E('ERR_HTTP2_SOCKET_BOUND',
    'The socket is already bound to an Http2Session', Error);
E('ERR_HTTP2_STATUS_101',
    'HTTP status code 101 (Switching Protocols) is forbidden in HTTP/2', Error);
E('ERR_HTTP2_STATUS_INVALID', 'Invalid status code: %s', RangeError);
E('ERR_HTTP2_STREAM_CANCEL', 'The pending stream has been canceled', Error);
E('ERR_HTTP2_STREAM_ERROR', 'Stream closed with error code %s', Error);
E('ERR_HTTP2_STREAM_SELF_DEPENDENCY',
    'A stream cannot depend on itself', Error);
E('ERR_HTTP2_TRAILERS_ALREADY_SENT',
    'Trailing headers have already been sent', Error);
E('ERR_HTTP2_TRAILERS_NOT_READY',
    'Trailing headers cannot be sent until after the wantTrailers event is ' +
    'emitted', Error);
E('ERR_HTTP2_UNSUPPORTED_PROTOCOL', 'protocol "%s" is unsupported.', Error);
E('ERR_HTTP_HEADERS_SENT',
    'Cannot %s headers after they are sent to the client', Error);
E('ERR_HTTP_INVALID_HEADER_VALUE',
    'Invalid value "%s" for header "%s"', TypeError);
E('ERR_HTTP_INVALID_STATUS_CODE', 'Invalid status code: %s', RangeError);
E('ERR_HTTP_TRAILER_INVALID',
    'Trailers are invalid with this transfer encoding', Error);
E('ERR_INDEX_OUT_OF_RANGE', 'Index out of range', RangeError);
E('ERR_INSPECTOR_ALREADY_CONNECTED', '%s is already connected', Error);
E('ERR_INSPECTOR_CLOSED', 'Session was closed', Error);
E('ERR_INSPECTOR_NOT_AVAILABLE', 'Inspector is not available', Error);
E('ERR_INSPECTOR_NOT_CONNECTED', 'Session is not connected', Error);
E('ERR_INVALID_ADDRESS_FAMILY', 'Invalid address family: %s', RangeError);
E('ERR_INVALID_ARG_TYPE', 'Invalid argument type', TypeError);
E('ERR_INVALID_ARG_VALUE', (name, value, reason = 'is invalid') => {
    let inspected = util.inspect(value);
    if (inspected.length > 128) {
        inspected = `${inspected.slice(0, 128)}...`;
    }
    return `The argument '${name}' ${reason}. Received ${inspected}`;
}, TypeError, RangeError);
E('ERR_INVALID_ARRAY_LENGTH',
    (name, len, actual) => {
        internalAssert(typeof actual === 'number', 'actual must be of type number');
        return `The array "${name}" (length ${actual}) must be of length ${len}.`;
    }, TypeError);
E('ERR_INVALID_ASYNC_ID', 'Invalid %s value: %s', RangeError);
E('ERR_INVALID_BUFFER_SIZE',
    'Buffer size must be a multiple of %s', RangeError);
E('ERR_INVALID_CALLBACK', 'Callback must be a function', TypeError);
E('ERR_INVALID_CHAR', 'Invalid character', TypeError);
E('ERR_INVALID_CURSOR_POS',
    'Cannot set cursor row without setting its column', TypeError);
E('ERR_INVALID_DOMAIN_NAME', 'Unable to determine the domain name', TypeError);
E('ERR_INVALID_FD',
    '"fd" must be a positive integer: %s', RangeError);
E('ERR_INVALID_FD_TYPE', 'Unsupported fd type: %s', TypeError);
E('ERR_INVALID_FILE_URL_HOST',
    'File URL host must be "localhost" or empty on %s', TypeError);
E('ERR_INVALID_FILE_URL_PATH', 'File URL path %s', TypeError);
E('ERR_INVALID_HANDLE_TYPE', 'This handle type cannot be sent', TypeError);
E('ERR_INVALID_HTTP_TOKEN', '%s must be a valid HTTP token ["%s"]', TypeError);
E('ERR_INVALID_IP_ADDRESS', 'Invalid IP address: %s', TypeError);
E('ERR_INVALID_OPT_VALUE', (name, value) =>
    `The value "${String(value)}" is invalid for option "${name}"`,
    TypeError,
    RangeError);
E('ERR_INVALID_OPT_VALUE_ENCODING',
    'The value "%s" is invalid for option "encoding"', TypeError);
E('ERR_INVALID_PERFORMANCE_MARK',
    'The "%s" performance mark has not been set', Error);
E('ERR_INVALID_PROTOCOL',
    'Protocol "%s" not supported. Expected "%s"',
    TypeError);
E('ERR_INVALID_REPL_EVAL_CONFIG',
    'Cannot specify both "breakEvalOnSigint" and "eval" for REPL', TypeError);
E('ERR_INVALID_RETURN_VALUE', (input, name, value) => {
  let type;
  if (value && value.constructor && value.constructor.name) {
    type = `instance of ${value.constructor.name}`;
  } else {
    type = `type ${typeof value}`;
  }
  return `Expected ${input} to be returned from the "${name}"` +
          ` function but got ${type}.`;
}, TypeError);
E('ERR_INVALID_SYNC_FORK_INPUT',
    'Asynchronous forks do not support Buffer, Uint8Array or string input: %s',
    TypeError);
E('ERR_INVALID_THIS', 'Value of "this" must be of type %s', TypeError);
E('ERR_INVALID_TUPLE', '%s must be an iterable %s tuple', TypeError);
E('ERR_INVALID_URI', 'URI malformed', URIError);
E('ERR_INVALID_URL', 'Invalid URL: %s', TypeError);
E('ERR_INVALID_URL_SCHEME',
    (expected) => `The URL must be ${oneOf(expected, 'scheme')}`, TypeError);
E('ERR_IPC_CHANNEL_CLOSED', 'Channel closed', Error);
E('ERR_IPC_DISCONNECTED', 'IPC channel is already disconnected', Error);
E('ERR_IPC_ONE_PIPE', 'Child process can have only one IPC pipe', Error);
E('ERR_IPC_SYNC_FORK', 'IPC cannot be used with synchronous forks', Error);
E('ERR_METHOD_NOT_IMPLEMENTED', 'The %s method is not implemented', Error);
E('ERR_MISSING_ARGS', 'Missing arguments', TypeError);
E('ERR_MISSING_MODULE', 'Cannot find module %s', Error);
E('ERR_MODULE_RESOLUTION_LEGACY',
    '%s not found by import in %s.' +
    ' Legacy behavior in require() would have found it at %s',
    Error);
E('ERR_MULTIPLE_CALLBACK', 'Callback called multiple times', Error);
E('ERR_NAPI_CONS_FUNCTION', 'Constructor must be a function', TypeError);
E('ERR_NAPI_INVALID_DATAVIEW_ARGS',
    'byte_offset + byte_length should be less than or equal to the size in ' +
    'bytes of the array passed in',
    RangeError);
E('ERR_NAPI_INVALID_TYPEDARRAY_ALIGNMENT',
    'start offset of %s should be a multiple of %s', RangeError);
E('ERR_NAPI_INVALID_TYPEDARRAY_LENGTH',
    'Invalid typed array length', RangeError);
E('ERR_NO_CRYPTO',
    'Node.js is not compiled with OpenSSL crypto support', Error);
E('ERR_NO_ICU',
    '%s is not supported on Node.js compiled without ICU', TypeError);
E('ERR_NO_LONGER_SUPPORTED', '%s is no longer supported', Error);
E('ERR_OUT_OF_RANGE', 'Out of range', RangeError);
E('ERR_REQUIRE_ESM', 'Must use import to load ES Module: %s', Error);
E('ERR_SCRIPT_EXECUTION_INTERRUPTED',
    'Script execution was interrupted by `SIGINT`.', Error);
E('ERR_SERVER_ALREADY_LISTEN',
    'Listen method has been called more than once without closing.', Error);
E('ERR_SERVER_NOT_RUNNING', 'Server is not running.', Error);
E('ERR_SOCKET_ALREADY_BOUND', 'Socket is already bound', Error);
E('ERR_SOCKET_BAD_BUFFER_SIZE',
    'Buffer size must be a positive integer', TypeError);
E('ERR_SOCKET_BAD_PORT',
    'Port should be > 0 and < 65536. Received %s.', RangeError);
E('ERR_SOCKET_BAD_TYPE',
    'Bad socket type specified. Valid types are: udp4, udp6', TypeError);
E('ERR_SOCKET_BUFFER_SIZE',
    'Could not get or set buffer size',
    Error);
E('ERR_SOCKET_CANNOT_SEND', 'Unable to send data', Error);
E('ERR_SOCKET_CLOSED', 'Socket is closed', Error);
E('ERR_SOCKET_DGRAM_NOT_RUNNING', 'Not running', Error);
E('ERR_STDERR_CLOSE', 'process.stderr cannot be closed', Error);
E('ERR_STDOUT_CLOSE', 'process.stdout cannot be closed', Error);
E('ERR_STREAM_CANNOT_PIPE', 'Cannot pipe, not readable', Error);
E('ERR_STREAM_DESTROYED', 'Cannot call %s after a stream was destroyed', Error);
E('ERR_STREAM_NULL_VALUES', 'May not write null values to stream', TypeError);
E('ERR_STREAM_PREMATURE_CLOSE', 'Premature close', Error);
E('ERR_STREAM_PUSH_AFTER_EOF', 'stream.push() after EOF', Error);
E('ERR_STREAM_UNSHIFT_AFTER_END_EVENT',
    'stream.unshift() after end event', Error);
E('ERR_STREAM_WRAP', 'Stream has StringDecoder set or is in objectMode', Error);
E('ERR_STREAM_WRITE_AFTER_END', 'write after end', Error);
E('ERR_SYSTEM_ERROR', 'A system error occurred', Error);
E('ERR_TLS_CERT_ALTNAME_INVALID',
    'Hostname/IP does not match certificate\'s altnames: %s', Error);
E('ERR_TLS_DH_PARAM_SIZE', 'DH parameter size %s is less than 2048', Error);
E('ERR_TLS_HANDSHAKE_TIMEOUT', 'TLS handshake timeout', Error);
E('ERR_TLS_RENEGOTIATION_DISABLED',
    'TLS session renegotiation disabled for this socket', Error);

// This should probably be a `TypeError`.
E('ERR_TLS_REQUIRED_SERVER_NAME',
    '"servername" is required parameter for Server.addContext', Error);
E('ERR_TLS_SESSION_ATTACK', 'TLS session renegotiation attack detected', Error);
E('ERR_TLS_SNI_FROM_SERVER',
    'Cannot issue SNI from a TLS server-side socket', Error);
E('ERR_TRACE_EVENTS_CATEGORY_REQUIRED',
    'At least one category is required', TypeError);
E('ERR_TRACE_EVENTS_UNAVAILABLE', 'Trace events are unavailable', Error);
E('ERR_TRANSFORM_ALREADY_TRANSFORMING',
    'Calling transform done when still transforming', Error);

// This should probably be a `RangeError`.
E('ERR_TRANSFORM_WITH_LENGTH_0',
    'Calling transform done when writableState.length != 0', Error);
E('ERR_TTY_INIT_FAILED', 'TTY initialization failed', Error);
E('ERR_UNCAUGHT_EXCEPTION_CAPTURE_ALREADY_SET',
    '`process.setupUncaughtExceptionCapture()` was called while a capture ' +
    'callback was already active',
    Error);
E('ERR_UNESCAPED_CHARACTERS', '%s contains unescaped characters', TypeError);
E('ERR_UNHANDLED_ERROR',
    // Using a default argument here is important so the argument is not counted
    // towards `Function#length`.
    (err = undefined) => {
        const msg = 'Unhandled error.';
        if (err === undefined) return msg;
        return `${msg} (${err})`;
    }, Error);
E('ERR_UNKNOWN_ENCODING', 'Unknown encoding: %s', TypeError);

// This should probably be a `TypeError`.
E('ERR_UNKNOWN_FILE_EXTENSION', 'Unknown file extension: %s', Error);
E('ERR_UNKNOWN_MODULE_FORMAT', 'Unknown module format: %s', RangeError);
E('ERR_UNKNOWN_SIGNAL', 'Unknown signal: %s', TypeError);
E('ERR_UNKNOWN_STDIN_TYPE', 'Unknown stdin file type', Error);

// This should probably be a `TypeError`.
E('ERR_UNKNOWN_STREAM_TYPE', 'Unknown stream file type', Error);
E('ERR_V8BREAKITERATOR',
    'Full ICU data not installed. See https://github.com/nodejs/node/wiki/Intl',
    Error);

// This should probably be a `TypeError`.
E('ERR_VALID_PERFORMANCE_ENTRY_TYPE',
    'At least one valid performance entry type is required', Error);
E('ERR_VM_MODULE_ALREADY_LINKED', 'Module has already been linked', Error);
E('ERR_VM_MODULE_DIFFERENT_CONTEXT',
    'Linked modules must use the same context', Error);
E('ERR_VM_MODULE_LINKING_ERRORED',
    'Linking has already failed for the provided module', Error);
E('ERR_VM_MODULE_NOT_LINKED',
    'Module must be linked before it can be instantiated', Error);
E('ERR_VM_MODULE_NOT_MODULE',
    'Provided module is not an instance of Module', Error);
E('ERR_VM_MODULE_STATUS', 'Module status %s', Error);
E('ERR_ZLIB_INITIALIZATION_FAILED', 'Initialization failed', Error);
