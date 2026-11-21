"""Read-only SEO checks; never invoke Sphinx or execute its configuration/extensions.

Set SPEEDCRUNCH_DOCS_ROOT when checking a source tree from outside this directory.
"""
import ast
import html
import json
import os
import re
import unittest
from pathlib import Path
from xml.etree import ElementTree

from docutils.core import publish_parts
from docutils.writers.html5_polyglot import Writer
from jinja2 import Environment, FileSystemLoader

ROOT = Path(os.environ.get('SPEEDCRUNCH_DOCS_ROOT',
                          str(Path(__file__).resolve().parents[1] / 'src')))
BASE = 'https://www.speedcrunch.org/'
TEMPLATES = ROOT / '_templates_standalone'
HOME_STATIC = ROOT / '_static_standalone' / 'home'


def config_assignments():
    """Top-level assignments of conf.py, parsed without running it."""
    config = ast.parse((ROOT / 'conf.py').read_text())
    return {node.targets[0].id: node.value for node in ast.walk(config)
            if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Name)}


class WebsiteSEO(unittest.TestCase):
    def setUp(self):
        self.env = Environment(loader=FileSystemLoader(TEMPLATES),
                               extensions=['jinja2.ext.i18n'])
        self.env.install_null_translations()
        self.env.filters['tobool'] = lambda value: bool(value)
        self.env.filters['toint'] = lambda value: int(value) if value else 0

    def context(self, name, description='', title='Manual topic'):
        return dict(
            pagename=name, pageurl=BASE + name + '.html', title=title,
            language='en_US', encoding='utf-8', release='1.0', version='1.0',
            project='SpeedCrunch', docstitle='SpeedCrunch 1.0 documentation',
            theme_bootstrap_version='3', theme_bootswatch_theme='',
            theme_nosidebar=True, embedded=False, sidebars=[],
            css_files=[], script_files=[], parents=[], master_doc='contents',
            theme_navbar_pagenav=False, theme_navbar_sidebarrel=False,
            theme_source_link_position='footer',
            metatags=('<meta name="description" content="' + html.escape(description)
                      + '" />') if description else '',
            hasdoc=lambda name: False, toctree=lambda **kwargs: '',
            pathto=lambda name, resource=False: name if resource else name + '.html',
        )

    def render(self, name, template='layout.html', **kwargs):
        return self.env.get_template(template).render(self.context(name, **kwargs))

    def test_all_templates_parse(self):
        for path in TEMPLATES.glob('*.html'):
            with self.subTest(template=path.name):
                self.env.get_template(path.name)

    def test_distinct_titles_descriptions_and_canonicals(self):
        pages = dict(index='home.html', download='download.html',
                     community='community.html', donate='donate.html')
        titles, descriptions = set(), set()
        for name, template in pages.items():
            output = self.render(name, template)
            title = re.findall(r'<title>(.*?)</title>', output)
            description = re.findall(r'<meta name="description" content="([^"]*)"', output)
            canonical = re.findall(r'<link rel="canonical" href="([^"]*)"', output)
            self.assertEqual(len(title), 1)
            self.assertEqual(len(description), 1)
            self.assertEqual(canonical, [BASE if name == 'index' else BASE + name + '.html'])
            self.assertIn('SpeedCrunch', title[0])
            titles.add(title[0])
            descriptions.add(description[0])
        self.assertEqual(len(titles), len(pages))
        self.assertEqual(len(descriptions), len(pages))

    def test_homepage_structured_data(self):
        output = self.render('index', 'home.html')
        schema = json.loads(re.search(r'<script type="application/ld\+json">(.*?)</script>',
                                      output, re.S).group(1))
        site, app = schema['@graph']
        self.assertEqual(site['@type'], 'WebSite')
        self.assertEqual(site['url'], BASE)
        self.assertEqual(app['@type'], 'SoftwareApplication')
        self.assertEqual(app['downloadUrl'], BASE + 'download.html')
        self.assertEqual(app['offers']['price'], 0)
        self.assertNotIn('aggregateRating', app)
        self.assertNotIn('review', app)
        # JSON interpolation must remain valid even for special characters.
        context = self.context('index')
        context['release'] = '1.0 "test" </script>'
        output = self.env.get_template('home.html').render(context)
        schema = json.loads(re.search(r'<script type="application/ld\+json">(.*?)</script>',
                                      output, re.S).group(1))
        self.assertEqual(schema['@graph'][1]['softwareVersion'], context['release'])

    def test_homepage_link_preview(self):
        output = self.render('index', 'home.html')
        meta = dict(re.findall(r'<meta (?:property|name)="((?:og|twitter):[\w:]+)" content="([^"]*)"',
                               output))
        self.assertEqual(meta['og:image'], BASE + 'og-image.jpg')
        self.assertEqual(meta['twitter:card'], 'summary_large_image')
        self.assertTrue(meta['og:image:alt'])
        extra = ast.literal_eval(config_assignments()['html_extra_path'])
        self.assertIn('og-image.jpg', extra)
        # The tags must match the image, and WhatsApp skips images over 300 KB.
        data = (ROOT / 'og-image.jpg').read_bytes()
        self.assertLess(len(data), 300 * 1000)
        self.assertEqual(data[:2], b'\xff\xd8')
        pos = 2
        while data[pos + 1] not in (0xC0, 0xC1, 0xC2):
            pos += 2 + int.from_bytes(data[pos + 2:pos + 4], 'big')
        height = int.from_bytes(data[pos + 5:pos + 7], 'big')
        width = int.from_bytes(data[pos + 7:pos + 9], 'big')
        self.assertEqual(meta['og:image:type'], 'image/jpeg')
        self.assertEqual((meta['og:image:width'], meta['og:image:height']),
                         (str(width), str(height)))
        self.assertEqual((width, height), (1200, 630))
        # Manual pages share no home page preview image.
        self.assertNotIn('og:image', self.render('introduction'))

    def test_additional_page_without_title_and_offline_context(self):
        context = self.context('index')
        del context['title']
        output = self.env.get_template('home.html').render(context)
        self.assertIn('<title>SpeedCrunch — Free High-Precision Scientific Calculator</title>', output)
        context['pageurl'] = None
        output = self.env.get_template('home.html').render(context)
        self.assertNotIn('rel="canonical"', output)
        self.assertNotIn('application/ld+json', output)

    def test_nested_manual_and_search_metadata(self):
        output = self.render('reference/basic', title='Math <em>&amp; units</em>',
                             description='Roots, "matrices" & units')
        self.assertIn('<title>Math &amp; units — SpeedCrunch Manual</title>', output)
        self.assertIn('href="' + BASE + 'reference/basic.html"', output)
        self.assertEqual(output.count('name="description"'), 1)
        self.assertNotIn('noindex', output)
        self.assertIn('lang="en-US"', output)
        self.assertNotIn('maximum-scale', output)
        self.assertIn('content="noindex, follow"', self.render('search'))
        self.assertNotIn('application/ld+json', output)

    def test_manual_descriptions_parse(self):
        paths = [ROOT / 'contents.rst', ROOT / 'introduction.rst',
                 *sorted((ROOT / 'userguide').glob('*.rst')),
                 *sorted((ROOT / 'reference').glob('*.rst'))]
        descriptions = set()
        for path in paths:
            if path.name == 'units_table.rst':
                continue
            with self.subTest(page=path.name):
                text = path.read_text()
                block = text.split('\n\n', 1)[0]
                self.assertTrue(block.startswith('.. meta::\n   :description: '))
                # Test only the metadata fixture, without loading Sphinx extensions.
                parts = publish_parts(block + '\n\nExample\n=======\n', writer=Writer())
                self.assertEqual(parts['meta'].count('name="description"'), 1)
                self.assertIn('<h1 class="title">Example</h1>', parts['html_body'])
                descriptions.add(block)
        self.assertEqual(len(descriptions), 17)

    def test_sitemap_and_config_coverage(self):
        tree = ElementTree.parse(ROOT / 'sitemap.xml')
        urls = [node.text for node in tree.findall('.//{*}loc')]
        sources = [ROOT / 'contents.rst', ROOT / 'introduction.rst', ROOT / 'faq.rst',
                   *sorted((ROOT / 'userguide').glob('*.rst')),
                   *sorted((ROOT / 'reference').glob('*.rst'))]
        expected = {BASE + path.relative_to(ROOT).with_suffix('.html').as_posix()
                    for path in sources if path.name != 'units_table.rst'}
        assignments = config_assignments()
        additional = ast.literal_eval(assignments['html_additional_pages'])
        expected.update(BASE if name == 'index' else BASE + name + '.html'
                        for name in additional)
        self.assertEqual(set(urls), expected)
        self.assertEqual(len(urls), len(expected))
        self.assertEqual(ast.literal_eval(assignments['html_baseurl']), BASE)
        extra = ast.literal_eval(assignments['html_extra_path'])
        self.assertIn('sitemap.xml', extra)
        self.assertIn('robots.txt', extra)
        self.assertTrue(all((ROOT / name).is_file() for name in extra))
        self.assertIn('Sitemap: ' + BASE + 'sitemap.xml', (ROOT / 'robots.txt').read_text())

    def test_homepage_links_and_download_headings(self):
        output = self.render('index', 'home.html')
        for path, anchor in re.findall(r'href="((?:userguide|reference)/[^"#]+)\.html#([^"]+)"', output):
            source = (ROOT / (path + '.rst')).read_text()
            headings = re.findall(r'^([^\n]+)\n[-=+]{3,}\s*$', source, re.M)
            heading_ids = {re.sub(r'[^a-z0-9]+', '-', heading.lower()).strip('-')
                           for heading in headings}
            labels = re.findall(r'^\s*\.\. _([^:\n]+):\s*$', source, re.M)
            label_ids = {re.sub(r'[^a-z0-9]+', '-', label.lower()).strip('-')
                         for label in labels}
            self.assertIn(anchor, heading_ids | label_ids, (path, anchor))
        download = self.render('download', 'download.html')
        self.assertIn('<title>Download SpeedCrunch', download)
        h1 = re.findall(r'<h1[^>]*>(.*?)</h1>', download)
        self.assertEqual(len(h1), 1)
        self.assertRegex(h1[0], r'^SpeedCrunch \d+\.\d+')
        # Older releases are collapsible panels; each must still be an <h2>.
        older = re.findall(r'<h2[^>]*>\s*<a[^>]*>(Version [^<]+)</a>\s*</h2>', download)
        self.assertIn('Version 1.0', older)
        self.assertIn('Version 0.12', older)
        self.assertNotRegex(download, r'<h[3-6][^>]*>\s*<a[^>]*>Version ')
        self.assertIn('id="older-versions"', download)
        # Links to headings stop under the top bar through CSS (scroll-padding
        # in extra.css); an inline margin as well would double the gap.
        self.assertNotIn('scroll-margin-top', download)

    def test_homepage_assets_links_and_script_hooks(self):
        output = self.render('index', 'home.html')
        # The home page has its own styles and scripts, without Bootstrap.
        self.assertNotIn('_static/bootstrap', output)
        self.assertNotIn('jquery', output)
        # Some links end in ?v=… so that browsers fetch the new file after a change.
        assets = re.findall(r'(?:src|href)="_static/home/([^"?]+)', output)
        for name in ('style.css', 'schemes.js', 'app.js'):
            self.assertIn(name, assets)
        for name in assets:
            self.assertTrue((HOME_STATIC / name).is_file(), name)
        # Links to other pages of the site must point to pages that exist.
        additional = ast.literal_eval(config_assignments()['html_additional_pages'])
        for page in re.findall(r'href="([\w/]+)\.html"', output):
            self.assertTrue(page in additional or (ROOT / (page + '.rst')).is_file(), page)
        # app.js fills in elements that the template provides.
        script = (HOME_STATIC / 'app.js').read_text()
        ids = set(re.findall(r'\bid="([^"]+)"', output))
        for name in re.findall(r'\$\("#([\w-]+)', script):
            self.assertIn(name, ids)
        schemes = set(re.findall(r'"name": "([^"]+)"', (HOME_STATIC / 'schemes.js').read_text()))
        for name in re.findall(r'data-scheme="([^"]+)"', output):
            self.assertIn(name, schemes)
        for name in re.findall(r'data-display="([^"]+)"', output):
            self.assertRegex(script, r'\n\s+' + name + r': \[')
        for os_name in ('windows', 'macos', 'linux'):
            self.assertEqual(output.count('data-' + os_name + '="'), 2)

    def test_download_platform_columns(self):
        download = self.render('download', 'download.html')
        starts = [m.start() for m in re.finditer(r'class="dl-platforms"', download)]
        sections = [download[a:b] for a, b in zip(starts, starts[1:] + [len(download)])]
        # 1.2 and 0.12 have Windows, macOS and Linux; 1.1 and 1.0 no Linux.
        self.assertEqual([len(re.findall(r'class="dl-platform"', s)) for s in sections], [3, 2, 2, 3])
        # The page's script finds the visitor's platform by these names.
        self.assertEqual(re.findall(r'data-platform="([^"]+)"', sections[0]),
                         ['windows', 'macos', 'linux'])
        self.assertIn("'.dl-platform[data-platform=\"' + os + '\"]'", download)
        latest = sections[0]
        self.assertEqual(latest.count('From the terminal'), 3)
        commands = re.findall(r'<code class="dl-cmd">([^<]*)</code>', latest)
        self.assertEqual(commands, [
            'winget install SpeedCrunch.SpeedCrunch',
            'scoop bucket add extras', 'scoop install extras/speedcrunch',
            'choco install speedcrunch.install',
            'choco install speedcrunch.portable',
            'brew install --cask speedcrunch',
            # Too long for a column: continued on the next line, as in a shell.
            'flatpak install flathub \\', '  org.speedcrunch.SpeedCrunch'])

    def test_font_awesome_is_served_locally(self):
        static = ROOT / '_static_standalone' / 'fontawesome'
        css = (static / 'css' / 'all.min.css').read_text()
        urls = set(re.findall(r'url\(([^)]+)\)', css))
        self.assertEqual(urls, {
            '../webfonts/fa-brands-400.woff2',
            '../webfonts/fa-regular-400.woff2',
            '../webfonts/fa-solid-900.woff2',
            '../webfonts/fa-v4compatibility.woff2',
        })
        self.assertTrue(all((static / 'css' / url).is_file() for url in urls))

    def test_light_and_dark_mode(self):
        pages = [('index', 'home.html'), ('download', 'download.html'),
                 ('community', 'community.html'), ('reference/basic', 'layout.html'),
                 ('search', 'search.html')]
        for name, template in pages:
            with self.subTest(page=name):
                output = self.render(name, template)
                themed = name != 'index'
                # The mode is set before the stylesheets load, so the page
                # never draws in the other mode first.
                script = output.find("setAttribute('data-theme'")
                self.assertEqual(script >= 0, themed)
                self.assertEqual('class="site-theme-toggle"' in output, themed)
                if themed:
                    self.assertLess(script, output.find('rel="stylesheet"'))
                    self.assertIn("prefers-color-scheme: dark", output)
                    self.assertIn("localStorage.getItem('sc-theme')", output)
                    # Without a saved light or dark choice, the setting is
                    # system: the page takes the system's mode.
                    self.assertIn("if (choice !== 'light' && choice !== 'dark') choice = 'system';", output)
        script = (ROOT / '_static_standalone' / 'site-nav.js').read_text()
        self.assertIn('localStorage.setItem("sc-theme"', script)
        # The toggle goes light, dark, system; system forgets the saved
        # choice and follows the system's changes at once.
        self.assertIn('{ light: "dark", dark: "system", system: "light" }', script)
        self.assertIn('localStorage.removeItem("sc-theme")', script)
        self.assertIn('addEventListener("change"', script)
        bar = self.render('download', 'download.html')
        for choice in ('light', 'dark', 'system'):
            self.assertIn('data-label-' + choice + '="', bar)

    def test_search_page_and_result_previews(self):
        # Sphinx search builds preview URLs from data-content_root and reads
        # the previews from each page's role="main" element.
        context = self.context('reference/basic')
        context['content_root'] = '../'
        output = self.env.get_template('layout.html').render(context)
        self.assertIn('data-content_root="../"', output)
        self.assertEqual(output.count('role="main"'), 1)
        # Older Sphinx versions have no content_root.
        self.assertNotIn('data-content_root', self.render('reference/basic'))
        # The warning must not need jQuery's global $, which jquery-fix.js removes.
        search = self.render('search', 'search.html')
        self.assertIn('<noscript>', search)
        self.assertNotIn('$(', search)
        self.assertIn('id="search-results"', search)
        self.assertIn('_static/searchtools.js', search)

    def test_same_top_bar_and_footer_on_every_page(self):
        pages = [('index', 'home.html'), ('download', 'download.html'),
                 ('community', 'community.html'), ('donate', 'donate.html'),
                 ('reference/basic', 'layout.html'), ('search', 'layout.html')]
        for name, template in pages:
            with self.subTest(page=name):
                output = self.render(name, template)
                bars = re.findall(r'<header class="site-nav[^"]*" id="navbar">(.*?)</header>',
                                  output, re.S)
                self.assertEqual(len(bars), 1)
                bar = bars[0]
                # The theme's sidebar code reads the height of ".footer".
                footers = re.findall(r'<footer class="footer site-footer">(.*?)</footer>',
                                     output, re.S)
                self.assertEqual(len(footers), 1)
                self.assertEqual(output.count('<footer'), 1)
                for page in ('introduction', 'faq', 'donate'):
                    self.assertIn('href="' + page + '.html"', footers[0])
                self.assertIn('_static/site.css', output)
                # Icons come with the website, not from another server.
                self.assertIn('href="_static/fontawesome/css/all.min.css"', output)
                self.assertNotIn('bootstrapcdn', output)
                self.assertIn('_static/site-nav.js', output)
                self.assertNotIn('navbar-fixed-top', output)
                for page in ('download', 'faq', 'community', 'donate'):
                    self.assertIn('href="' + page + '.html"', bar)
                # The home and manual pages open the chapters from "User Manual";
                # manual pages can also search, except the search page itself.
                manual = name not in ('index', 'download', 'community', 'donate')
                chapters = manual or name == 'index'
                self.assertEqual('site-chapters-menu' in bar, chapters)
                self.assertEqual('href="introduction.html"' in bar, not chapters)
                self.assertEqual('class="site-search"' in bar, manual and name != 'search')
                # The theme's script rewrites lists with this class.
                self.assertNotIn('globaltoc', bar)
                current = re.findall(r'<a href="([^"]+)" aria-current="page"', bar)
                self.assertEqual(current, [name + '.html'] if name in ('download', 'community', 'donate') else [])


if __name__ == '__main__':
    unittest.main(verbosity=2)
