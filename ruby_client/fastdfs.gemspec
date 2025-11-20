# FastDFS Ruby Client Gemspec
#
# This file defines the gem specification for the FastDFS Ruby client.
# It includes metadata, dependencies, and file lists for the gem package.
#
# # Copyright (C) 2025 FastDFS Ruby Client Contributors
#
# FastDFS may be copied only under the terms of the GNU General
# Public License V3, which may be found in the FastDFS source kit.

# Require RubyGems
# This is needed for gem specification
require_relative 'lib/fastdfs'

# Gem specification
# This defines the gem metadata and structure
Gem::Specification.new do |spec|
  # Basic gem information
  # These are required fields for a gem specification
  
  # Gem name
  # This must be unique on RubyGems
  spec.name = 'fastdfs'
  
  # Gem version
  # This follows semantic versioning: MAJOR.MINOR.PATCH
  spec.version = FastDFS::VERSION
  
  # Gem authors
  # List of people who created this gem
  spec.authors = ['FastDFS Ruby Client Contributors']
  
  # Gem email
  # Contact email for gem issues
  spec.email = ['384681@qq.com']
  
  # Gem description
  # Short description of what the gem does
  spec.description = 'Ruby client library for FastDFS distributed file system'
  
  # Gem summary
  # One-line summary of the gem
  spec.summary = 'Ruby client for FastDFS - A high-performance distributed file system'
  
  # Gem homepage
  # URL to the project homepage or repository
  spec.homepage = 'https://github.com/happyfish100/fastdfs'
  
  # Gem license
  # License for the gem code
  spec.license = 'GPL-3.0'
  
  # Required Ruby version
  # Minimum Ruby version required
  spec.required_ruby_version = '>= 2.7.0'
  
  # Files included in the gem
  # List of files to package with the gem
  spec.files = Dir[
    # Library files
    'lib/**/*.rb',
    
    # Example files
    'examples/**/*.rb',
    
    # Documentation files
    'README.md',
    'LICENSE',
    'CHANGELOG.md',
    
    # Gem specification
    'fastdfs.gemspec',
    
    # Gemfile for dependencies
    'Gemfile',
    'Gemfile.lock'
  ].reject { |f| f.match(%r{^(test|spec|features)/}) }
  
  # Test files
  # Files used for testing (not included in gem)
  spec.test_files = Dir['test/**/*.rb', 'spec/**/*.rb']
  
  # Executables
  # Executable scripts included with the gem
  spec.executables = []
  
  # Require paths
  # Paths to add to $LOAD_PATH when requiring files
  spec.require_paths = ['lib']
  
  # Runtime dependencies
  # Gems required at runtime
  # Currently no external dependencies required
  # Standard library only (socket, timeout, thread, etc.)
  
  # Development dependencies
  # Gems required only for development and testing
  spec.add_development_dependency 'rake', '~> 13.0'
  spec.add_development_dependency 'minitest', '~> 5.0'
  spec.add_development_dependency 'rubocop', '~> 1.0'
  spec.add_development_dependency 'yard', '~> 0.9'
  
  # Metadata
  # Additional metadata for RubyGems
  spec.metadata = {
    # Source code repository
    'source_code_uri' => 'https://github.com/happyfish100/fastdfs',
    
    # Bug tracker
    'bug_tracker_uri' => 'https://github.com/happyfish100/fastdfs/issues',
    
    # Documentation
    'documentation_uri' => 'https://github.com/happyfish100/fastdfs/blob/master/ruby_client/README.md',
    
    # Changelog
    'changelog_uri' => 'https://github.com/happyfish100/fastdfs/blob/master/ruby_client/CHANGELOG.md',
    
    # Ruby version requirements
    'rubygems_mfa_required' => 'false'
  }
  
  # Post-install message
  # Message to display after gem installation
  spec.post_install_message = <<-MESSAGE
  Thank you for installing FastDFS Ruby Client!
  
  For usage examples, see:
    https://github.com/happyfish100/fastdfs/tree/master/ruby_client/examples
  
  For documentation, see:
    https://github.com/happyfish100/fastdfs/blob/master/ruby_client/README.md
  MESSAGE
end

