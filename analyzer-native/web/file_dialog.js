// The JS shim for the native file dialog.
//
// Browsers cannot open a blocking file dialog, so the WASM build asks the
// browser for a file with a hidden <input type="file"> element, copies the
// selected file into Emscripten's in-memory filesystem and hands its path back
// to C++. The rest of the app can then open it with the normal std::ifstream
// based parser.
//
// The function is marked as async, which makes Emscripten wrap it with
// Asyncify.handleAsync and suspend the WASM stack until the file was picked.

addToLibrary({
  kbi_open_recording_dialog__deps: ['$FS', '$stringToUTF8'],
  kbi_open_recording_dialog__async: 'auto',
  kbi_open_recording_dialog: (pathPtr, pathSize, startDirectoryPtr) => (async () => {
    const file = await new Promise((resolve) => {
      const input = document.createElement('input');
      input.type = 'file';
      // Both formats the native analyzer supports.
      input.accept = '.json,.kbi,application/json';
      input.style.display = 'none';
      document.body.appendChild(input);

      let settled = false;
      const finish = (selected) => {
        if (settled) return;
        settled = true;
        window.removeEventListener('focus', onFocus);
        input.remove();
        resolve(selected);
      };
      const onFocus = () => {
        // The "cancel" event is not supported by every browser. When the
        // window gets the focus back and no file was chosen, treat the
        // dialog as cancelled.
        setTimeout(() => {
          if (settled || (input.files && input.files.length > 0)) return;
          finish(null);
        }, 1000);
      };

      input.addEventListener('change', () => finish(input.files[0] || null), { once: true });
      input.addEventListener('cancel', () => finish(null), { once: true });
      window.addEventListener('focus', onFocus);

      // Must run synchronously from the user gesture that opened the dialog,
      // Asyncify resumes this function once the browser closed the picker.
      input.click();
    });

    if (!file) return 0;

    const contents = new Uint8Array(await file.arrayBuffer());
    const directory = '/tmp/kbi';
    FS.mkdirTree(directory);
    // Keep the name unique so that picking two files with the same name does
    // not overwrite the one that is still being analysed.
    const path = `${directory}/${Date.now()}-${file.name}`;
    FS.writeFile(path, contents);
    stringToUTF8(path, pathPtr, pathSize);
    return 1;
  })(),
});
