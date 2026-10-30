import json
import sys
import threading
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pr_overlap

URL = "repos/owner/project/pulls?state=open&base=main&per_page=100"


def pull(number):
    return {"number": number, "user": {"login": f"author{number}"}, "head": {"sha": f"sha{number}"}, "body": None}


def response(numbers, last=None, newline="\r\n"):
    headers = ["HTTP/2.0 200 OK", "Content-Type: application/json; charset=utf-8"]
    if last:
        headers.append(f'Link: <https://api.github.com/repositories/1/pulls?state=open&base=main&per_page=100&page=2>; rel="next", '
                       f'<https://api.github.com/repositories/1/pulls?state=open&base=main&per_page=100&page={last}>; rel="last"')
    return newline.join(headers) + newline + newline + json.dumps([pull(n) for n in numbers])


class ServedPages:
    def __init__(self, pages, last):
        self.pages = pages
        self.last = last
        self.requested = []
        self.lock = threading.Lock()

    def __call__(self, *args):
        self.assertion(args)
        number = int(args[2].rsplit("&page=", 1)[1])
        with self.lock:
            self.requested.append(number)
        return response(self.pages.get(number, []), self.last if number == 1 else None)

    @staticmethod
    def assertion(args):
        if args[:2] != ("api", "--include") or not args[2].startswith(URL + "&page="):
            raise AssertionError(f"unexpected gh call {args}")


class ParallelPageTests(unittest.TestCase):
    def open_prs(self, served):
        with patch.object(pr_overlap, "REPO", "owner/project", create=True), \
                patch.object(pr_overlap, "gh", side_effect=served), \
                patch.object(pr_overlap, "progress"):
            return pr_overlap.open_prs("main")

    def test_every_page_named_by_the_last_link_is_fetched(self):
        pages = {1: range(1, 101), 2: range(101, 201), 3: range(201, 251)}
        served = ServedPages(pages, last=3)
        found = self.open_prs(served)
        self.assertEqual(set(found), set(range(1, 251)))
        self.assertCountEqual(served.requested, [1, 2, 3])
        self.assertEqual(found[250], {"ref": "refs/pr/250", "sha": "sha250", "author": "author250", "depends": set()})

    def test_a_single_page_needs_one_request(self):
        served = ServedPages({1: range(1, 43)}, last=None)
        self.assertEqual(set(self.open_prs(served)), set(range(1, 43)))
        self.assertEqual(served.requested, [1])

    def test_next_links_without_a_last_link_are_followed_even_on_short_pages(self):
        pages = {1: range(1, 3), 2: range(3, 5), 3: range(5, 6)}
        served = ServedPages(pages, last=None)

        def next_only(*args):
            result = served(*args)
            number = int(args[2].rsplit("&page=", 1)[1])
            if number < 3:
                link = f'Link: <https://api.github.com/repositories/1/pulls?page={number + 1}&per_page=100>; rel="next"'
                result = result.replace("\r\n\r\n", "\r\n" + link + "\r\n\r\n", 1)
            return result

        found = self.open_prs(next_only)
        self.assertEqual(set(found), set(range(1, 6)))
        self.assertEqual(served.requested, [1, 2, 3])

    def test_next_only_full_final_page_checks_for_a_new_following_page(self):
        pages = {1: range(1, 101), 2: range(101, 201), 3: range(201, 202)}
        served = ServedPages(pages, last=None)

        def next_only(*args):
            result = served(*args)
            number = int(args[2].rsplit("&page=", 1)[1])
            if number == 1:
                link = 'Link: <https://api.github.com/repositories/1/pulls?page=2&per_page=100>; rel="next"'
                result = result.replace("\r\n\r\n", "\r\n" + link + "\r\n\r\n", 1)
            return result

        self.assertEqual(set(self.open_prs(next_only)), set(range(1, 202)))
        self.assertEqual(served.requested, [1, 2, 3])

    def test_invalid_or_repeated_next_pages_are_rejected(self):
        for query in ("per_page=100", "page=one", "page=0", "page=1", "page=2&page=3", "page=&page=2"):
            with self.subTest(query=query):
                link = f'Link: <https://api.github.com/repositories/1/pulls?{query}>; rel="next"'
                output = response(range(1, 3)).replace("\r\n\r\n", "\r\n" + link + "\r\n\r\n", 1)
                with patch.object(pr_overlap, "REPO", "owner/project", create=True), \
                        patch.object(pr_overlap, "gh", return_value=output):
                    with self.assertRaisesRegex(ValueError, "invalid next page link"):
                        pr_overlap.open_prs("main")

    def test_a_full_last_page_is_followed_by_the_next_one(self):
        pages = {1: range(1, 101), 2: range(101, 201), 3: range(201, 202)}
        served = ServedPages(pages, last=2)
        self.assertEqual(set(self.open_prs(served)), set(range(1, 202)))
        self.assertEqual(sorted(served.requested), [1, 2, 3])

    def test_a_full_last_page_followed_by_an_empty_one_stops(self):
        served = ServedPages({1: range(1, 101), 2: range(101, 201)}, last=2)
        self.assertEqual(set(self.open_prs(served)), set(range(1, 201)))
        self.assertEqual(sorted(served.requested), [1, 2, 3])

    def test_a_pull_request_shifted_onto_two_pages_is_kept_once(self):
        served = ServedPages({1: range(1, 101), 2: range(100, 150)}, last=2)
        found = self.open_prs(served)
        self.assertEqual(len(found), 149)

    def test_headers_with_plain_newlines_are_split_from_the_body(self):
        with patch.object(pr_overlap, "gh", return_value=response(range(1, 3), newline="\n")):
            head, body = pr_overlap.page(URL + "&page=1")
        self.assertTrue(head.startswith("HTTP/2.0 200 OK"))
        self.assertEqual(body, json.dumps([pull(1), pull(2)]))

    def test_a_non_array_page_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "non-array page"):
            pr_overlap.json_pages('{"message": "Not Found"}')


if __name__ == "__main__":
    unittest.main()
