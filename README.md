# diffpick

diffpick is a portable Qt 6 launcher for selecting two or three files, folders,
or pasted texts and opening the comparison in Meld.

See the [user guide](docs/user-guide.md) for examples, Browse actions, saved
locations, file manager integration, and all keyboard shortcuts.

## Windows portable package

Extract the complete ZIP to a permanent folder and run `diffpick.exe`. Meld must
be installed separately or configured through **Menu > Settings**.

To add Explorer context-menu and Send To entries, choose
**Menu > System integration > Install**. Use **Repair** after moving the extracted
folder. Before deleting diffpick, choose **Remove** so no stale Explorer entries
remain.

## Linux AppImage

Make the AppImage executable and run it directly. Meld must be installed
separately, either as a native package or as the Flatpak `org.gnome.meld`.

System integration can be installed from **Menu > System integration > Install**.
Keep the AppImage in a permanent location after installing the integration; use
**Repair** if it is moved.

## Keyboard shortcuts

- Alt+L, Alt+R, Alt+M: open the Left, Right, or Middle input menu
- Alt+S: swap Left and Right
- Alt+C or Ctrl+Enter: compare
- Ctrl+1, Ctrl+2, Ctrl+3: focus Left, Middle, or Right

diffpick does not compute diffs itself. It starts Meld with argument lists and
does not pass paths through a shell.

## License

diffpick is released under the [MIT License](LICENSE). Qt and bundled
third-party runtime components retain their respective licenses; their notices
are included in the distributed packages.

See the [Code signing policy](docs/code-signing.md) for Windows release signing.
