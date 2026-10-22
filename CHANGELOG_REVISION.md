# Revision notes

## Implemented in this revision

- Expanded the illustrative Pune-area road graph with 20 intermediate junction/approach nodes (74 nodes in total in the seeded graph).
- Added matching node coordinates and route-segment definitions so route polylines can display the added turns rather than skipping them.
- Changed server binding to loopback by default; same-Wi-Fi access is an explicit `MEDEVAC_LAN=1` opt-in.
- Added a console warning when LAN mode is enabled because the demo uses plain HTTP and fixed demonstration credentials.
- Removed wildcard CORS response headers for the same-origin browser app and added basic response security headers.
- Qualified the socket `bind` call as `::bind`.
- Updated README run instructions, architecture notes, and security limitations.
- Added a detailed theory document covering architecture, TCP/HTTP, SQLite, transactions, Dijkstra, allocation, security, testing, alternatives and viva questions.

## Verification performed

- C++17 compilation succeeded using the bundled SQLite source.
- Local smoke test: controller page returned HTTP 200.
- Local smoke test: incorrect controller credentials returned HTTP 401.
- Local smoke test: configured demo controller credentials returned a successful login response with a bearer token.
- Consistency check: all 74 backend graph nodes have map coordinates; all backend road edges have a corresponding UI route segment.

## Still not production-ready

- Demo credentials are fixed in source and shared by demo users.
- Plain HTTP is used; LAN mode is only for a trusted classroom network.
- Hospital state endpoint is demo-only and lacks hospital-specific authentication.
- Session expiry/revocation, login rate limiting, TLS, secure password hashing, comprehensive audit logging and production hardening are not complete.
- The road network and travel weights are synthetic; no live GPS or traffic is connected.
- Multi-device connectivity must be tested on actual separate devices and is not claimed as verified by the local smoke test.
