"""
FastDFS Python Client Setup Script
"""

from setuptools import setup, find_packages
from pathlib import Path

# Read README for long description
readme_file = Path(__file__).parent / 'README.md'
long_description = readme_file.read_text(encoding='utf-8') if readme_file.exists() else ''

setup(
    name='fastdfs-client',
    version='1.0.0',
    author='FastDFS Python Client Contributors',
    author_email='fastdfs@example.com',
    description='Official Python client for FastDFS distributed file system',
    long_description=long_description,
    long_description_content_type='text/markdown',
    url='https://github.com/happyfish100/fastdfs',
    project_urls={
        'Bug Reports': 'https://github.com/happyfish100/fastdfs/issues',
        'Source': 'https://github.com/happyfish100/fastdfs/tree/master/python_client',
        'Documentation': 'https://github.com/happyfish100/fastdfs/blob/master/python_client/README.md',
    },
    packages=find_packages(exclude=['tests', 'tests.*', 'examples', 'examples.*']),
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'Topic :: Software Development :: Libraries :: Python Modules',
        'Topic :: System :: Filesystems',
        'License :: OSI Approved :: GNU General Public License v3 (GPLv3)',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3.9',
        'Programming Language :: Python :: 3.10',
        'Programming Language :: Python :: 3.11',
        'Programming Language :: Python :: 3.12',
        'Operating System :: OS Independent',
    ],
    python_requires='>=3.7',
    install_requires=[
        # No external dependencies - uses only Python standard library
    ],
    extras_require={
        'dev': [
            'pytest>=7.0.0',
            'pytest-cov>=4.0.0',
            'black>=23.0.0',
            'flake8>=6.0.0',
            'mypy>=1.0.0',
            'isort>=5.12.0',
        ],
    },
    entry_points={
        'console_scripts': [
            # Add CLI tools here if needed
        ],
    },
    include_package_data=True,
    zip_safe=False,
    keywords='fastdfs distributed-file-system storage client',
)