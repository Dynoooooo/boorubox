# BooruBox

BooruBox is a Linux-first desktop GUI for searching Booru-style image APIs,
previewing cached thumbnails, downloading selected posts, and indexing the local
archive for later browsing.

The application code is C++23. The GUI uses Qt 6 Widgets in C++; the backend
uses libcurl, nlohmann/json, SQLite/JSONL indexing, and Catch2 tests. The local
archive defaults to SQLite when the index path ends in `.sqlite`, `.sqlite3`, or
`.db`; SQLite itself is a C library, but all BooruBox application code remains
C++. A JSONL index remains available by setting `index_path` to a `.jsonl` file.

## License

BooruBox is licensed under the GNU Affero General Public License version 3. See
`LICENSE` for the full license text.

## Safety Defaults

BooruBox defaults to SFW mode:

- `enable_nsfw = false`
- NSFW-only providers are hidden.
- Search URLs add safe-only rating filters where the provider supports them.
- Parsed results are filtered again before they can be downloaded.
- Blacklisted tags are sent as query exclusions and enforced locally.

When `enable_nsfw = true`, BooruBox shows a one-time GUI warning before the main
window starts. Do not use BooruBox to access, search for, download, organize, or
preserve illegal sexual content, sexualized minors, non-consensual sexual
content, content that violates site rules, or content behind access controls. The
app does not bypass logins, bans, Cloudflare, age gates, paywalls, or region
blocks.

Use a clear User-Agent in your config, for example:

```toml
user_agent = "BooruBox/0.1 (+https://your.repo.example/boorubox; you@example.com)"
```

Never put API keys or passwords in source control.

## Linux Dependencies

Ubuntu/Debian package names:

```bash
sudo apt install build-essential cmake git pkg-config libcurl4-openssl-dev
sudo apt install qt6-base-dev qt6-base-dev-tools libsqlite3-dev
```

CMake fetches nlohmann/json and Catch2 at configure time. JSONL indexing works
without SQLite if `index_path` is changed to a `.jsonl` file and the SQLite index
class is not used.

## Prebuilt Linux Release

GitHub releases include a Linux x86_64 tarball with the `boorubox` executable,
`LICENSE`, `README.md`, and `config.example.toml`.

The current tarball is dynamically linked. On Ubuntu/Debian, install the runtime
libraries first:

```bash
sudo apt install libqt6widgets6 qt6-qpa-plugins libcurl4 libsqlite3-0
```

Then unpack and run:

```bash
tar -xzf boorubox-v0.1.0-linux-x86_64.tar.gz
cd boorubox-v0.1.0-linux-x86_64
./boorubox
```

Because BooruBox is AGPLv3, each release is tied to a source tag in this GitHub
repository.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/boorubox
```

Optional local sanitizer run:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DBOORUBOX_SANITIZERS=address,undefined
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

Use a custom config path:

```bash
./build/boorubox ./config.example.toml
```

The main executable is a Qt desktop app. It opens a normal window with sidebar
navigation for Search, Results, Queue, Gallery, Settings, and Logs.

## GUI MVP

Implemented:

- Search page with provider selector, tags, exclusions, rating, page, limit,
  search, clear, SFW mode toggle, and SFW/NSFW indicator.
- Pressing Enter on the Search page runs the same action as the Search button.
- Results page with a thumbnail grid, multi-select, metadata panel, preview
  panel, download selected/all visible, open post URL, and Use as Reference.
- Preview loading through the existing preview cache. Preview/sample URLs are
  preferred; full originals are not downloaded just to preview.
- Queue page with status, progress bars, speed, error messages, retry failed, and
  cancel selected.
- Queue cleanup can clear failed and skipped jobs, including skipped duplicate
  downloads, without touching active downloads.
- Gallery page with local thumbnails, provider/rating/favorite filters, tag
  search, sorting, image preview, open file, Use as Reference, and rebuild index.
- Reference images from Results or Gallery are shown on the Search page. Their
  metadata tags are displayed but are only copied into the Tags field when the
  user clicks Use Reference Tags.
- Settings page that loads/saves the config, including `enable_nsfw`,
  directories, User-Agent, rating, download quality, blacklist, and provider
  toggles. Provider enable/disable changes apply live; downloader structural
  changes apply after restart.
- Rating controls hide sensitive/questionable/explicit choices while SFW mode is
  active.
- Logs page that polls the existing in-memory logger and supports Copy All.

## Roadmap

Next GUI polish order:

1. Results: richer thumbnail cache state and duplicate/skipped indicators.
2. Queue: clear completed and better ETA formatting.
3. Gallery: persisted favorites and richer metadata editing.
4. Settings: live provider reload, file picker buttons, and safer secret display.
5. Logs: persistent log file and filtered severity view.

## Initial Provider API Research

Research was checked against current public documentation before implementing the
MVP.

### Danbooru

- Base URL: `https://danbooru.donmai.us`
- Search endpoint: `GET /posts.json`
- Query parameters: `tags`, `limit`, `page`, optional `random`
- Pagination: numeric `page`; docs also support `a<ID>` / `b<ID>` keyset style
- Rating filtering: search metatags such as `rating:g`, `rating:q`, `rating:e`
- Response: JSON array of post records
- File fields: `file_url`, `large_file_url`, `preview_file_url`
- Metadata fields: `md5`, `image_width`, `image_height`, `file_ext`,
  `file_size`, `tag_string`, `tag_string_artist`, `score`, `fav_count`
- Authentication: not required for public read-only search; API keys exist for
  authenticated actions
- Rate limit: docs state a global read request limit; BooruBox uses a conservative
  one-request-per-second default
- Docs: <https://shima.donmai.us/wiki_pages/help%3Aapi>,
  <https://shima.donmai.us/wiki_pages/api%3Aposts>

### Gelbooru-Compatible DAPI

- Base URL: `https://gelbooru.com`
- Search endpoint: `GET /index.php?page=dapi&s=post&q=index`
- Query parameters: `tags`, `limit`, `pid`, `json=1`
- Pagination: `pid`, treated as zero-based by the MVP
- Rating filtering: tag metatags; MVP uses `rating:general` in SFW mode
- Response: JSON object with a `post` array/object, or array on some compatible
  sites
- File fields: `file_url`, `sample_url`, `preview_url`
- Metadata fields: `id`, `md5`/`hash`, `width`/`image_width`,
  `height`/`image_height`, `tags`, `rating`, `score`
- Authentication: public reads are available, but Gelbooru docs note API auth
  may be required or throttled in some cases
- Docs: <https://gelbooru.com/index.php?page=help&topic=dapi>,
  <https://gelbooru.com/index.php?id=18780&page=wiki&s=view>

### Safebooru

- Base URL: `https://safebooru.org`
- Search endpoint: `GET /index.php?page=dapi&s=post&q=index`
- Query parameters: `tags`, `limit`, `pid`, `json=1`
- Pagination: `pid`
- Rating filtering: BooruBox still sends `rating:general` in SFW mode
- Response: Gelbooru-style DAPI JSON
- Limit note: Safebooru documents a hard post-list limit of 1000
- Docs: <https://safebooru.org/index.php?page=help&topic=dapi>

### e926/e621

- Base URLs: `https://e926.net` for SFW-first, `https://e621.net` for explicit
  provider config
- Search endpoint: `GET /posts.json`
- Query parameters: `tags`, `limit`, `page`
- Pagination: numeric `page`, with docs recommending `page=b<ID>` for large
  iteration
- Rating filtering: `rating:safe`, `rating:questionable`, `rating:explicit`
- Response: JSON object with `posts`
- File fields: nested `file.url`, `sample.url`, `preview.url`
- Metadata fields: nested `file.width`, `file.height`, `file.ext`,
  `file.size`, `file.md5`, `score.total`, `tags.artist`, `tags.general`,
  `fav_count`, `sources`
- Authentication: not required for basic public read-only search; credentials are
  supported by the site for account actions
- User-Agent/rate limit: e621/e926 require a descriptive non-browser User-Agent;
  docs state a hard limit of two requests per second and recommend no more than
  one request per second sustained
- Docs: <https://e621.net/help/api>,
  <https://e621.net/wiki_pages/help%3Acheatsheet>

## Architecture

Implemented modules:

- `app`: config loading, safety acknowledgement, provider/download/index wiring
- `providers`: `SiteProvider`, Danbooru, Gelbooru-compatible, Safebooru, e926/e621
- `http`: libcurl RAII client
- `download`: queued download manager, `.part` files, retries, resume, filename
  templates
- `index`: archive interface, JSONL implementation, optional SQLite implementation
- `preview`: preview cache and preview backend abstractions
- `gui`: Qt Widgets main window and pages
- `tests`: URL building, JSON parsing, filename sanitization, blacklist filtering,
  duplicate detection, archive indexing

## Adding A Provider

1. Add a class deriving from `SiteProvider`.
2. Implement provider-specific `build_search_request`.
3. Implement `parse_search_response` and `normalize_post`.
4. Add conservative safe-mode rating terms and blacklist exclusions.
5. Register the provider in `App::register_providers`.
6. Add config under `[providers.name]`.
7. Add URL-building and JSON parsing tests.

Do not assume Booru APIs share field names or pagination semantics.

## Folder Structure

```text
boorubox/
  CMakeLists.txt
  README.md
  config.example.toml
  src/
    main.cpp
    app/
    gui/
    http/
    providers/
    model/
    download/
    preview/
    index/
    util/
  tests/
```
