#!/usr/bin/env perl

use strict;
use warnings;

use lib ('.');

use File::Basename;

my $vhome = '.';

use BRLCAD_DOC;

die "Enter book name.\n" if !@ARGV;

my $typ   = shift @ARGV;
my $debug = scalar @ARGV ? 1 : 0;

# provision for a draft overlay
my $draft = shift @ARGV;
$draft = defined $draft ? 1 : 0;

# known names

# the autogenerated file:
my $nam = $typ;
$nam = 'books/en/BRL-CAD_Tutorial_Series-VolumeI.xml'
  if $nam eq 'dummy.xml';

# trim extra stuff to leave a unique name
$nam =~ s{\.xml \z}{}xmsi;
$nam =~ s{\A books/en/}{}xmsi;

my $ofil = "book-covers-fo-autogen.xsl";

my %name
  = (
     'BRL-CAD_Tutorial_Series-VolumeI' => {
					   title_1 => 'BRL-CAD Tutorial Series:',
					   title_2 => 'Volume I - An Overview of BRL-CAD',
					  },
     'BRL-CAD_Tutorial_Series-VolumeII' => {
					   title_1 => 'BRL-CAD Tutorial Series:',
					   title_2 => 'Volume II - Introduction to MGED',
					   },
     'BRL-CAD_Tutorial_Series-VolumeIII' => {
					   title_1 => 'BRL-CAD Tutorial Series:',
					   title_2 => 'Volume III - Principles of Effective Modeling',
					    },
     'BRL-CAD_Tutorial_Series-VolumeIV' => {
					   title_1 => 'BRL-CAD Tutorial Series:',
					   title_2 => 'Volume IV - Converting Geometry Between BRL-CAD and Other Formats',
					   },
     'dummy' => 1,
    );

if (!exists $name{$nam}) {
  die "Unknown file for cover '$typ'.";
}

my $title_1 = $name{$nam}{title_1};
my $title_2 = $name{$nam}{title_2};

# the covers' template source:
my $cvr = 'book-covers-fo-template.xsl';

open my $fpi, '<', $cvr
  or die "$cvr: $!";

open my $fp, '>', $ofil
  or die "$ofil: $!";

print "DEBUG: output file = '$ofil'\n"
  if $debug;

# special first line handling
my $line = <$fpi>;
if ($line =~ m{\A \s* \<\?xml}xms) {
  print $fp $line;
  BRLCAD_DOC::print_autogen_header($fp, $0);
}
else {
  print "Unexpected first line:\n";
  print $line;
  die "Unable to continue";
}

my $need_brlcad_logo_group = 0;
my $need_draft             = $draft;
my $need_title             = 1;

while (defined(my $line = <$fpi>)) {

  if ($need_draft
      && $line =~ m{\A \s* \<\?brlcad \s+ insert\-draft\-overlay \s* \?\>}xms) {
    BRLCAD_DOC::print_draft_overlay($fp);
    $need_draft = 0;
    next;
  }

  if ($need_brlcad_logo_group
      && $line =~ m{\A \s* \<\?brlcad \s+ insert\-brlcad\-logo\-group \s* \?\>}xms) {
    BRLCAD_DOC::print_brlcad_logo_group($fp);
    $need_brlcad_logo_group = 0;
    next;
  }

  if ($need_title
      && $line =~ m{\A \s* \<\?brlcad \s+ insert\-title \s* \?\>}xms) {
    BRLCAD_DOC::print_book_title($fp, $title_1, $title_2);
    $need_title = 0;
    next;
  }

  print $fp $line;
}
