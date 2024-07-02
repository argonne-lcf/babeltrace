# SPDX-FileCopyrightText: 2017 Philippe Proulx <pproulx@efficios.com>
# SPDX-License-Identifier: MIT

import sys
import time
import shlex
import logging
import pathlib
import argparse
import subprocess
import xml.etree.ElementTree
from typing import List, Optional


class _Laurence:
    def __init__(self, session_name: str, max_output_dir_size: int):
        self._session_name = session_name
        self._max_output_dir_size = max_output_dir_size

    def run(self):
        logging.info(
            f"Starting Laurence daemon for recording session `{self._session_name}` and maximum output directory size {self._max_output_dir_size_str}"
        )
        self._set_session_output_dir()
        logging.info(
            f"Found recording session `{self._session_name}` with output directory `{self._output_dir}`"
        )
        consuming: Optional[bool] = None

        while True:
            logging.info(f"Waiting {_Laurence._retry_delay} s...")
            self._sleep()

            if self._session_status_elem is None:
                logging.info(
                    f"Recording session `{self._session_name}` doesn't exist anymore: quitting"
                )
                return

            if not self._output_dir.exists():
                logging.warning(
                    f"Output directory `{self._output_dir}` doesn't exist yet"
                )
                continue

            output_dir_size = self._output_dir_size
            output_dir_size_str = self._format_mib(output_dir_size)
            logging.info(f"Current output directory size is {output_dir_size_str}")

            if output_dir_size > self._max_output_dir_size:
                logging.info(
                    f"Current size of output directory ({output_dir_size_str}) is greater than the configured maximum value ({self._max_output_dir_size_str})"
                )

                if consuming is None or consuming:
                    logging.info(
                        f"Stopping the consumer for recording session `{self._session_name}`"
                    )
                    consuming = False
                    _Laurence._run_lttng_ignore_errors(["pause", self._session_name])
            else:
                logging.info(
                    f"Current size of output directory ({output_dir_size_str}) is less than or equal to the configured maximum value ({self._max_output_dir_size_str})"
                )

                if consuming is None or not consuming:
                    logging.info(
                        f"Starting the consumer for recording session `{self._session_name}`"
                    )
                    _Laurence._run_lttng_ignore_errors(["resume", self._session_name])
                    consuming = True

    @staticmethod
    def _format_mib(size: int):
        return f"{size / 2**20:.3f} MiB"

    @property
    def _max_output_dir_size_str(self):
        return _Laurence._format_mib(self._max_output_dir_size)

    @property
    def _output_dir_size(self):
        while True:
            try:
                total = 0

                for file in self._output_dir.rglob("*"):
                    total += file.stat().st_size

                return total
            except:
                continue

    @staticmethod
    def _run_lttng(args: List[str]):
        args = ["lttng"] + args
        logging.info(f"Running: {shlex.join(args)}")
        return subprocess.check_output(args)

    @staticmethod
    def _run_lttng_ignore_errors(args: List[str]):
        try:
            return _Laurence._run_lttng(args)
        except subprocess.CalledProcessError:
            logging.warning(f"`lttng {shlex.join(args)}`: ignoring error")

    @staticmethod
    def _run_lttng_mi(args: List[str]):
        return xml.etree.ElementTree.fromstring(
            _Laurence._run_lttng(["--mi=xml"] + args)
        )

    _lttng_ns = {
        "": "https://lttng.org/xml/ns/lttng-mi",
    }

    @staticmethod
    def _find_known_in_elem(elem: xml.etree.ElementTree.Element, match: str):
        subelem = elem.find(match, _Laurence._lttng_ns)
        assert subelem is not None
        return subelem

    @staticmethod
    def _find_all_in_elem(elem: xml.etree.ElementTree.Element, match: str):
        return elem.findall(match, _Laurence._lttng_ns)

    @property
    def _session_status_elem(self):
        for session_elem in self._find_all_in_elem(
            self._run_lttng_mi(["list"]), "./output/sessions/*"
        ):
            if (
                self._find_known_in_elem(session_elem, "name").text
                == self._session_name
            ):
                return session_elem

    _retry_delay = 1

    @staticmethod
    def _sleep():
        time.sleep(_Laurence._retry_delay)

    def _set_session_output_dir(self):
        while True:
            session_elem = self._session_status_elem

            if session_elem is not None:
                break

            logging.warning(
                f"Recording session `{self._session_name}` doesn't seem to exist: trying again in {_Laurence._retry_delay} s..."
            )
            self._sleep()

        self._output_dir = pathlib.Path(
            str(self._find_known_in_elem(session_elem, "path").text)
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="Laurence",
        description="Output directory size monitoring daemon to control LTTng data consumption",
    )
    parser.add_argument(
        "-l",
        "--log-level",
        choices=["NOTSET", "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"],
        default="info",
        help="Log level",
    )
    parser.add_argument("session", help="Name of recording session to watch")
    parser.add_argument("maxsize", help="Maximum output directory size (MiB)")
    args = parser.parse_args()
    logging.basicConfig(
        level=args.log_level.upper(), format="[%(asctime)s] [%(levelname)s] %(message)s"
    )
    _Laurence(args.session, int(args.maxsize) * 2**20).run()
