import argparse
import time
import subprocess
from pathlib import Path
from playwright.sync_api import sync_playwright, expect


def run(args):
    output = Path(args.screenshots)
    output.mkdir(parents=True, exist_ok=True)
    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(headless=True)
        page = browser.new_page(viewport={'width': 1366, 'height': 768}, reduced_motion='reduce')
        errors = []
        page.on('pageerror', lambda error: errors.append(str(error)))
        page.goto(args.url)
        expect(page.locator('#pairing')).to_be_visible()
        page.screenshot(path=str(output / 'first-run.png'), full_page=True)
        code = subprocess.run([args.helper, '--pair', args.config], capture_output=True, text=True, check=True).stdout.strip()
        assert len(code) == 8 and code.isdecimal()
        page.get_by_label('Desktop pairing code').fill(code)
        page.get_by_role('button', name='Pair browser', exact=True).click()
        expect(page.locator('#library')).to_be_visible()
        expect(page.locator('#token')).to_have_value('')
        page.screenshot(path=str(output / 'library-laptop.png'), full_page=True)
        page.get_by_role('button', name='Add folder', exact=False).first.click()
        expect(page.get_by_role('dialog', name='')).to_be_visible()
        page.get_by_label('Windows folder path').fill(' "' + args.folder + '" ')
        page.get_by_role('button', name='Scan folder', exact=True).click()
        expect(page.locator('#scan-progress')).to_contain_text('Scan complete', timeout=30000)
        expect(page.locator('.result')).not_to_have_count(0)
        page.screenshot(path=str(output / 'scan-review.png'), full_page=True)
        if args.add:
            page.get_by_role('button', name='Add selected games', exact=True).click()
            expect(page.locator('#results')).to_contain_text('Already in library', timeout=15000)
        page.get_by_role('button', name='Close add folder').click()
        page.screenshot(path=str(output / 'library-populated.png'), full_page=True)
        if args.exercise_fixtures:
            fixture = Path(args.folder)
            assert 'Phase1-isolated' in str(fixture)
            expect(page.locator('.game')).to_have_count(2)
            previous = page.locator('.game').filter(has_text='Previous session needs confirmation')
            if previous.count():
                time.sleep(13)
                for card in previous.all():
                    card.get_by_role('button', name='Details', exact=True).click()
                    page.get_by_role('button', name='I have closed this game', exact=True).click()
                    expect(page.locator('#details')).to_contain_text('Available to launch', timeout=15000)
                    page.get_by_role('button', name='Close game details').click()
            def scan_path(path, expected):
                page.locator('header .add-folder').click()
                page.get_by_label('Windows folder path').fill(str(path))
                page.get_by_role('button', name='Scan folder', exact=True).click()
                expect(page.locator('#scan-progress')).to_contain_text(expected, timeout=30000)
            scan_path(fixture, 'Scan complete')
            expect(page.locator('.result')).to_have_count(3)
            expect(page.locator('#results')).to_contain_text('Needs preparation')
            expect(page.locator('#results input:checked')).to_have_count(0)
            page.screenshot(path=str(output / 'preparation-and-dedup.png'), full_page=True)
            page.get_by_role('button', name='Close add folder').click()
            scan_path(fixture.parent / 'Empty', 'No games found')
            page.screenshot(path=str(output / 'no-matches.png'), full_page=True)
            page.get_by_role('button', name='Close add folder').click()
            page.locator('header .add-folder').click()
            page.get_by_label('Windows folder path').fill(str(fixture.parent / 'Does not exist'))
            page.get_by_role('button', name='Scan folder', exact=True).click()
            expect(page.locator('#scan-error')).to_contain_text('missing or inaccessible', timeout=15000)
            page.screenshot(path=str(output / 'missing-path.png'), full_page=True)
            page.get_by_role('button', name='Close add folder').click()
            game_folder = fixture / 'Copy one'
            moved = fixture / 'Copy one.offline'
            game_folder.rename(moved)
            try:
                expect(page.locator('.game').filter(has_text='Copy one')).to_contain_text('missing', timeout=15000)
                page.screenshot(path=str(output / 'missing-game.png'), full_page=True)
            finally:
                moved.rename(game_folder)
            expect(page.locator('.game').filter(has_text='Copy one')).to_contain_text('Available', timeout=15000)
            slow = fixture.parent / 'Cancellation tree'
            for outer in range(140):
                for inner in range(140):
                    (slow / str(outer) / str(inner)).mkdir(parents=True, exist_ok=True)
            page.locator('header .add-folder').click()
            page.get_by_label('Windows folder path').fill(str(slow))
            page.get_by_role('button', name='Scan folder', exact=True).click()
            expect(page.get_by_role('button', name='Cancel scan', exact=True)).to_be_visible(timeout=15000)
            page.screenshot(path=str(output / 'scan-active.png'), full_page=True)
            page.get_by_role('button', name='Cancel scan', exact=True).click()
            expect(page.locator('#scan-progress')).to_contain_text('cancelled', timeout=15000)
            page.get_by_role('button', name='Close add folder').click()
            page.get_by_role('button', name='Folders', exact=False).first.click()
            page.once('dialog', lambda dialog: dialog.accept())
            page.locator('.folder').filter(has_text=str(slow)).get_by_role('button', name='Remove root').click()
            expect(page.locator('.folder').filter(has_text=str(slow))).to_have_count(0, timeout=15000)
            page.get_by_role('button', name='Rescan all folders', exact=True).click()
            expect(page.locator('#scan-progress')).to_contain_text('Scan complete', timeout=30000)
            expect(page.locator('.result')).to_have_count(3)
            page.get_by_role('button', name='Close add folder').click()
            page.get_by_role('button', name='Library', exact=False).first.click()
            failing_script = fixture / 'Copy one/Launch.cmd'
            original_script = failing_script.read_bytes()
            failing_script.write_text('@echo off\nexit /b 7\n')
            try:
                card = page.locator('.game').filter(has_text='Copy one')
                card.get_by_role('button', name='Play', exact=False).click()
                expect(card).to_contain_text('Exited with code 7', timeout=20000)
                page.screenshot(path=str(output / 'launch-failure.png'), full_page=True)
            finally:
                failing_script.write_bytes(original_script)
            try:
                failing_script.write_text('@echo off\npowershell.exe -NoProfile -Command "Start-Sleep -Seconds 12"\nexit /b 0\n')
                card = page.locator('.game').filter(has_text='Copy one')
                card.get_by_role('button', name='Play', exact=False).click()
                expect(card).to_contain_text('Running on Windows', timeout=15000)
                helper_path = str(Path(args.helper).resolve()).replace("'", "''")
                command = "Get-Process AnyPS5DesktopHelper | Where-Object { $_.Path -eq '" + helper_path + "' } | Stop-Process"
                subprocess.run(['powershell.exe', '-NoProfile', '-Command', command], check=True, capture_output=True)
                try:
                    expect(page.locator('#notice')).to_contain_text('disconnected', timeout=25000)
                    expect(page.locator('.game').first.locator('.primary')).to_be_disabled()
                    page.screenshot(path=str(output / 'helper-offline.png'), full_page=True)
                finally:
                    subprocess.Popen([args.helper, '--config', args.config], creationflags=subprocess.CREATE_NO_WINDOW)
                expect(page.locator('#connection')).to_have_text('Desktop connected', timeout=15000)
                expect(page.locator('.game')).to_have_count(2)
                expect(card).to_contain_text('Previous session needs confirmation', timeout=15000)
                card.get_by_role('button', name='Details', exact=True).click()
                expect(page.locator('#details')).to_contain_text('Session status unknown')
                page.screenshot(path=str(output / 'session-uncertain.png'), full_page=True)
                page.get_by_role('button', name='I have closed this game', exact=True).click()
                expect(page.locator('#details')).to_contain_text('Available to launch', timeout=15000)
                page.get_by_role('button', name='Close game details').click()
            finally:
                failing_script.write_bytes(original_script)
            print('PASS: plain Unicode path, review/add, copies with same title ID, duplicate rescan, preparation, empty results, missing path, missing game/recovery, real scan cancellation, root removal/rescan, nonzero exit, disconnected helper/reconnect, uncertain session confirmation', flush=True)
        if args.launch:
            for title in args.launch:
                card = page.locator('.game').filter(has=page.get_by_role('heading', name=title, exact=True))
                card.get_by_role('button', name='Play', exact=False).click()
                expect(card).to_contain_text('Running on Windows', timeout=15000)
                print('LAUNCHED', title, flush=True)
                expect(card).to_contain_text('Available', timeout=180000)
                print('EXIT OBSERVED', title, flush=True)
        page.get_by_role('button', name='Details', exact=True).first.click()
        expect(page.locator('#details')).to_be_visible()
        page.screenshot(path=str(output / 'game-details.png'), full_page=True)
        page.get_by_role('button', name='Close game details').click()
        page.set_viewport_size({'width': 390, 'height': 844})
        assert page.locator('body').evaluate('(element) => element.scrollWidth <= innerWidth')
        page.screenshot(path=str(output / 'library-narrow.png'), full_page=True)
        page.get_by_role('button', name='Folders', exact=False).first.click()
        page.screenshot(path=str(output / 'folders-narrow.png'), full_page=True)
        page.set_viewport_size({'width': 1366, 'height': 768})
        page.get_by_role('button', name='Library', exact=False).first.click()
        page.keyboard.press('Tab')
        assert page.locator(':focus-visible').count() == 1
        assert not errors, errors
        print('PASS: first-run, helper-issued one-use pairing, quoted paste, scan review, library, details, navigation, 1366x768, 390x844, keyboard focus; no JavaScript errors', flush=True)
        browser.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--url', default='http://127.0.0.1:8766')
    parser.add_argument('--helper', required=True)
    parser.add_argument('--config', required=True)
    parser.add_argument('--folder', required=True)
    parser.add_argument('--screenshots', required=True)
    parser.add_argument('--exercise-fixtures', action='store_true')
    parser.add_argument('--add', action='store_true')
    parser.add_argument('--launch', nargs='*')
    run(parser.parse_args())
