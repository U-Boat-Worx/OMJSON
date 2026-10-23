# Info
Package is provided by Loupe  
https://loupe.team  
info@loupe.team  
1-800-240-7042  

# Description

This repo provides packages for communicating with a B&R PLC using WebSockets and a JSON interface. This allows the PLC to act as a server for a Loupe UX client. 

### Library Package
The OMJSON library package bundles up the function blocks needed in order to run the WebSocket server and read/write to PLC variables.  

### Program Package
This repo also contains a program package that can be deployed to run the OMJSON-based webserver.

# Installation

To install the program package using the Loupe Package Manager (LPM), in an initialized Automation Studio project directory run `lpm install luxprog`. Note that this will also pull in the library package as a dependency. 
If you only want to install the library package, run `lpm install omjson`. 

For more information about LPM, see https://loupeteam.github.io/LoupeDocs/tools/lpm.html.

# Security

The servers in this library have **no encryption**, and no authentication unless you give the WebSocket server users (section 2). Any client that can open a TCP connection to the server port (default 8000) can send requests, and a browser will let any web page open a WebSocket to the PLC. By default, clients can read **and write** every variable on the PLC by name.

Before using this on a machine:

### 1. Restrict which variables clients can access

Pass an access list to the server. Variables that are not covered by an entry can neither be read nor written. An entry covers the variable and everything below it (structure members, array elements). If more than one entry matches, the most specific one wins.

```
VAR
	hmiAccess : jsonAccess_typ;
	hmiAccessList : ARRAY[0..2] OF jsonAccessEntry_typ;
END_VAR

hmiAccessList[0].name := 'gHmi';            hmiAccessList[0].access := JSON_ACCESS_READ;
hmiAccessList[1].name := 'gHmi.cmd';        hmiAccessList[1].access := JSON_ACCESS_READWRITE;
hmiAccessList[2].name := 'gHmi.par';        hmiAccessList[2].access := JSON_ACCESS_READWRITE;

hmiAccess.pEntries := ADR(hmiAccessList);
hmiAccess.numEntries := 3;
jsonWebSocketServer_0.pAccess := ADR(hmiAccess);
```

- Write entry names the canonical way (`'gVar'`, `'Task:var'`, `'gVar.member'`, `'gArray[2]'`). Case is ignored, as in IEC.
- `'gHmi'` covers `gHmi.cmd.start` and `gHmi[0]`, but not `gHmiOther`. Reading `gHmi` when only `gHmi.cmd` is listed is denied.
- A client must spell names canonically too: identifiers, `.member`, and `[index]` with a plain decimal index (no sign, spaces or leading zeros). Any other spelling is denied, because it could name the same variable without matching the entry meant for it (`gArray[02]` past an entry for `gArray[2]`). An entry that is not spelled canonically never matches. Application-module names (`::module::var`) and dereferenced references (`*ref`) cannot be served with an access list.
- A denied read returns `"undefined"`, the same as a variable that does not exist. A denied write is skipped; other values in the same request are still written.
- `hmiAccess.deniedReadCount`, `deniedWriteCount`, `lastDeniedName` and `lastDeniedClientIP` show what was rejected, and the server reports `JSON_ERR_ACCESSDENIED`.
- `hmiAccess.disableWrites := TRUE` makes the server read only. `hmiAccess.allowedClientIP[]` limits which clients may connect.
- **If `pAccess` is 0 there are no restrictions.** The server output `AccessControlActive` is FALSE in that case; consider raising an alarm on it. With `pAccess` set and `numEntries = 0`, everything is denied.

When using the luxprog program package, fill `gLuxAccessList[]` from your application instead. As long as `gLuxAccessList[0]` is empty, all variables are accessible.

### 2. Require a login before a client may write (WebSocket server)

Give the access list users, and give the entries that need one a minimum login level for writing. A client logs in over its own WebSocket connection, and the login belongs to that connection only: another client, or a new connection from the same browser, starts at level 0.

```
VAR
	hmiUsers : ARRAY[0..1] OF jsonUser_typ;
END_VAR

hmiUsers[0].name := 'service'; hmiUsers[0].salt := 'x7Q2-service'; hmiUsers[0].level := 1;
hmiUsers[0].hash := '<64 hex digits: SHA-256 of salt followed by the password>';
hmiUsers[1].name := 'factory'; hmiUsers[1].salt := 'k9P4-factory'; hmiUsers[1].level := 2;
hmiUsers[1].hash := '<...>';

hmiAccessList[1].writeLevel := 1; (* gHmi.cmd: writable after a login *)

hmiAccess.pUsers := ADR(hmiUsers);
hmiAccess.numUsers := 2;
hmiAccess.loginTimeout := 600000; (* ms without a write or touch; 0 = JSON_DEFAULT_LOGINTIMEOUT, also 10 min *)
```

| Client sends | Server answers |
|---|---|
| `{"type":"login","data":{"user":"service","password":"..."}}` | `{"type":"loginresponse","data":{"ok":true,"level":1,"client":0}}` |
| `{"type":"logout","data":{}}` | `{"type":"logoutresponse","data":{"ok":true,"level":0,"client":0}}` |
| `{"type":"touch","data":{}}` | `{"type":"touchresponse","data":{"ok":true,"level":1,"client":0}}`: restarts the login timeout and reports the level, 0 when not logged in |

A refused login answers `{"ok":false,"error":"...","level":L,"client":0}`. With error `denied` (wrong user or password) the connection's login has ended and `level` is 0. With `locked` (too many failures), `disabled` (no users configured) or `request` (malformed request) the connection keeps the login it had, and `level` reports it. `client` is the connection's index in `ClientInfo[]`. Logout and touch always answer `ok`.

- `hash` is the SHA-256 of `salt` followed by the password, as 64 hex digits, for example `node -e "console.log(require('crypto').createHash('sha256').update(process.argv[1] + process.argv[2]).digest('hex'))" <salt> <password>`. Give every user its own salt and its own name: only the first user with a name is ever checked.
- A write needs `Level >= writeLevel` of the most specific matching entry. Reads still follow `access` only. Clients of jsonHTTPServer, and jsonWriteVariable calls that do not set `ClientLevel`, are level 0, so with no users a `writeLevel > 0` entry is writable by nobody.
- A login ends on logout, on a failed login on the same connection, after `loginTimeout` ms without a write or touch (polling reads do not count), and when the connection closes. `ClientInfo[i].Level` and `ClientInfo[i].User` show each connection's login.
- Failed logins, from any client, are forgotten `loginLockout` ms (default 30 s) after the last one; a successful login does not clear them. While `JSON_LOGIN_MAX_FAILURES` (5) are counted, every login is refused. That allows 5 guesses per 30 s, about 14 400 a day, which decides how long a password must be. It also lets any client keep everyone locked out by failing 5 times every 30 s.
- `loginCount`, `loginFailCount`, `logoutCount`, `loginTimeoutCount`, `lastLoginUser` and `lastLoginClientIP` can feed an event log. `lastLoginUser` keeps the name of failed attempts too, which may be a password typed into the wrong field: do not let clients read it.
- User names are at most 31 characters, passwords at most 63; both printable ASCII without `"` or `\`. Longer or other values are refused, not shortened. The request data, which holds the password, is wiped from the receive buffer once it is handled.
- **The login is not encrypted**: anyone who can watch the network can read the password. SHA-256 is fast, so a short password can be found from its salt and hash by trying every candidate; keep the PLC project, which holds them, private.

### 3. Expose an interface structure, not your machine variables

Give the HMI its own structure (for example `gHmi.status`, `gHmi.par`, `gHmi.cmd`) and list only that. Let PLC code copy status into it, and validate and limit parameters and commands before passing them on to the machine. That way a client can never put a value into the control logic that the PLC did not check first. Do not put pointers or references into this structure.

### 4. Restrict the network

- Set `ServerIP` to the interface the HMI is connected to. If it is empty the server listens on all interfaces.
- Add a firewall rule in the Automation Runtime configuration that only allows the HMI to reach the server port.
- If the PLC is reachable from an office network or remotely, put a gateway that provides TLS in front of it, or use OPC UA for that connection. Without TLS a login only keeps out clients that do not know the password; it does not protect the password itself.

# Documentation
For more documentation and examples, see https://loupeteam.github.io/LoupeDocs/libraries/omjson.html (or you can run `lpm docs omjson`).

# Licensing

This project is licensed under the [MIT License](LICENSE).