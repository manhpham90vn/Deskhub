#!/usr/bin/env python3
import os
import pathlib
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
ANDROID_PROJECT = ROOT / "client" / "android"
CONFIGURATION = "releaseRuntimeClasspath"
APACHE_NAMES = {"apache-2.0", "the apache software license, version 2.0", "the apache license, version 2.0",
                "apache license, version 2.0", "apache license 2.0", "apache 2.0", "apache 2"}
NOTICE_NAME = re.compile(r"^(notice|license|licence|copying)(\.(txt|md))?$", re.IGNORECASE)
SKIPPED_CLASSIFIERS = ("-sources.jar", "-javadoc.jar", "-samples-sources.jar")
APACHE_END = "END OF TERMS AND CONDITIONS"
APACHE_APPENDIX = "APPENDIX: How to apply the Apache License to your work."
COPYRIGHT_LINE = re.compile(r"^\W*(Copyright\b.*\d{4}.*?)\s*$", re.MULTILINE)
LIBYUV_URL = "https://chromium.googlesource.com/libyuv/libyuv/+/refs/heads/main/README.chromium"
LIBYUV_LICENCE = """libyuv, built into the native image-processing library of this artifact:

Copyright 2011 The LibYuv Project Authors. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

  * Neither the name of Google nor the names of its contributors may
    be used to endorse or promote products derived from this software
    without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."""
KNOWN_LICENCE_TEXTS = {LIBYUV_URL: LIBYUV_LICENCE}
RULE = "=" * 78


def gradle_cache():
    home = os.environ.get("GRADLE_USER_HOME") or pathlib.Path.home() / ".gradle"
    return pathlib.Path(home) / "caches" / "modules-2" / "files-2.1"


def dependency_tree():
    wrapper = "gradlew.bat" if os.name == "nt" else "./gradlew"
    return subprocess.run([wrapper, "-q", ":app:dependencies", "--configuration", CONFIGURATION],
                          cwd=ANDROID_PROJECT, check=True, capture_output=True, text=True).stdout


def resolved_coordinates(tree):
    coordinates = set()
    for line in tree.splitlines():
        entry = re.sub(r"^[|+\\ -]+", "", line)
        if entry == line or entry.endswith("(c)") or entry.endswith("(n)"):
            continue
        entry = re.sub(r" \(\*\)$", "", entry)
        match = re.match(r"^([^:\s]+):([^:\s]+)(?::(\S+))?(?: -> (\S+))?", entry)
        if not match:
            continue
        group, name, requested, selected = match.groups()
        version = selected or requested
        if version:
            coordinates.add((group, name, version))
    return coordinates


def cached_files(group, name, version):
    directory = gradle_cache() / group / name / version
    return sorted(directory.glob("*/*")) if directory.is_dir() else []


def binary_of(files):
    for path in files:
        if path.suffix in (".aar", ".jar") and not path.name.endswith(SKIPPED_CLASSIFIERS):
            return path
    return None


def pom_path(group, name, version):
    for path in cached_files(group, name, version):
        if path.suffix == ".pom":
            return path
    sys.exit(f"android-library-notices.py: no POM for {group}:{name}:{version} in {gradle_cache()} - "
             f"build the Android app once so Gradle downloads it.")


def pom_of(group, name, version):
    return ET.parse(pom_path(group, name, version)).getroot()


def child(element, tag):
    return element.find(f"{{*}}{tag}")


def text_of(element, tag):
    found = child(element, tag)
    return found.text.strip() if found is not None and found.text else ""


def parent_pom(pom):
    parent = child(pom, "parent")
    if parent is None:
        return None
    return pom_of(text_of(parent, "groupId"), text_of(parent, "artifactId"), text_of(parent, "version"))


def inherited(pom, read):
    while pom is not None:
        value = read(pom)
        if value:
            return value
        pom = parent_pom(pom)
    return []


def licences(pom):
    return inherited(pom, lambda p: [(text_of(entry, "name"), text_of(entry, "url"))
                                     for entry in p.findall("{*}licenses/{*}license")])


def pom_copyrights(group, name, version):
    text = pom_path(group, name, version).read_text(encoding="utf-8", errors="replace")
    return list(dict.fromkeys(COPYRIGHT_LINE.findall(text)))


def holders(pom):
    def read(p):
        organisation = child(p, "organization")
        names = [text_of(organisation, "name")] if organisation is not None else []
        names += [text_of(d, "name") or text_of(d, "organization") for d in p.findall("{*}developers/{*}developer")]
        return [n for n in dict.fromkeys(names) if n]
    return inherited(pom, read)


def notice_entries(archive):
    texts = []
    for entry in sorted(archive.namelist()):
        if entry.endswith(".jar"):
            with zipfile.ZipFile(archive.open(entry)) as nested:
                texts += notice_entries(nested)
        elif entry.upper().startswith("META-INF/") and NOTICE_NAME.match(entry.rsplit("/", 1)[-1]):
            texts.append(archive.read(entry).decode("utf-8", errors="replace").strip())
    return [text for text in texts if not is_plain_apache_licence(text)]


def is_plain_apache_licence(text):
    flat = " ".join(text.split())
    if not flat.startswith("Apache License Version 2.0, January 2004") or APACHE_END not in flat:
        return False
    rest = flat.split(APACHE_END, 1)[1].strip()
    return not rest or rest.startswith(APACHE_APPENDIX)


def is_apache(name):
    return name.lower() in APACHE_NAMES


def section(group, name, version, binary):
    pom = pom_of(group, name, version)
    declared = licences(pom)
    if not declared:
        sys.exit(f"android-library-notices.py: {group}:{name}:{version} declares no licence in its POM - "
                 "its notice has to be written by hand before it can be distributed.")
    with zipfile.ZipFile(binary) as archive:
        notices = list(dict.fromkeys(notice_entries(archive)))
    for licence, url in declared:
        if is_apache(licence):
            continue
        if url not in KNOWN_LICENCE_TEXTS:
            sys.exit(f"android-library-notices.py: {group}:{name}:{version} is also under {licence} ({url}) - "
                     "add its licence text to KNOWN_LICENCE_TEXTS before it can be distributed.")
        notices.append(KNOWN_LICENCE_TEXTS[url])
    notices = pom_copyrights(group, name, version) + notices
    names = [licence for licence, _ in declared]
    title = text_of(pom, "name") or name
    lines = [f"{group}:{name}:{version}", f"{title}", f"Licence: {' / '.join(names)}"]
    url = text_of(pom, "url")
    if url:
        lines.append(f"Project: {url}")
    authors = holders(pom)
    if authors:
        lines.append(f"Authors: {', '.join(authors)}")
    body = "\n\n".join(notices) if notices else "No NOTICE file ships with this library."
    return f"{RULE}\n" + "\n".join(lines) + f"\n{RULE}\n\n{body}\n"


def main():
    if len(sys.argv) != 1:
        sys.exit("usage: android-library-notices.py > licenses/android-libraries.txt")
    sections = []
    for group, name, version in sorted(resolved_coordinates(dependency_tree())):
        binary = binary_of(cached_files(group, name, version))
        if binary is None:
            continue
        sections.append(section(group, name, version, binary))
    print("Java and Kotlin libraries packaged into the Deskhub Android APK (the Apache-2.0\n"
          f"text: see Apache-2.0.txt). Generated by scripts/android-library-notices.py from the\n"
          f"{CONFIGURATION} of client/android.\n")
    print("\n".join(sections), end="")


main()
