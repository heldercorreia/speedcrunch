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
        pages = dict(index='entrance.html', download='download.html',
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
        output = self.render('index', 'entrance.html')
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
        output = self.env.get_template('entrance.html').render(context)
        schema = json.loads(re.search(r'<script type="application/ld\+json">(.*?)</script>',
                                      output, re.S).group(1))
        self.assertEqual(schema['@graph'][1]['softwareVersion'], context['release'])

    def test_additional_page_without_title_and_offline_context(self):
        context = self.context('index')
        del context['title']
        output = self.env.get_template('entrance.html').render(context)
        self.assertIn('<title>SpeedCrunch — Free High-Precision Scientific Calculator</title>', output)
        context['pageurl'] = None
        output = self.env.get_template('entrance.html').render(context)
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
        sources = [ROOT / 'contents.rst', ROOT / 'introduction.rst',
                   *sorted((ROOT / 'userguide').glob('*.rst')),
                   *sorted((ROOT / 'reference').glob('*.rst'))]
        expected = {BASE + path.relative_to(ROOT).with_suffix('.html').as_posix()
                    for path in sources if path.name != 'units_table.rst'}
        config = ast.parse((ROOT / 'conf.py').read_text())
        assignments = {node.targets[0].id: node.value for node in ast.walk(config)
                       if isinstance(node, ast.Assign) and isinstance(node.targets[0], ast.Name)}
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
        output = self.render('index', 'entrance.html')
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
        self.assertEqual(re.findall(r'<h1[^>]*>(.*?)</h1>', download), ['Download SpeedCrunch'])
        self.assertIn('<h2>Version 1.0</h2>', download)
        self.assertIn('<h2 id="older-versions"', download)


if __name__ == '__main__':
    unittest.main(verbosity=2)
