"""Capture identifier-present examples without overwriting group 99 images."""

from capture_images import main


if __name__ == "__main__":
    main(default_presence_label=1)
