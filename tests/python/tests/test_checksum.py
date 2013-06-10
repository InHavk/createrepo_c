import unittest
import shutil
import tempfile
import os.path
import createrepo_c as cr

from fixtures import *

class TestCaseChecksum(unittest.TestCase):

    def test_checksum_name_str(self):
        self.assertEqual(cr.checksum_name_str(cr.MD5), "md5")
        self.assertEqual(cr.checksum_name_str(cr.SHA), "sha")
        self.assertEqual(cr.checksum_name_str(cr.SHA1), "sha1")
        self.assertEqual(cr.checksum_name_str(cr.SHA224), "sha224")
        self.assertEqual(cr.checksum_name_str(cr.SHA256), "sha256")
        self.assertEqual(cr.checksum_name_str(cr.SHA384), "sha384")
        self.assertEqual(cr.checksum_name_str(cr.SHA512), "sha512")
        self.assertEqual(cr.checksum_name_str(65), None)

    def test_checksum_type(self):
        self.assertEqual(cr.checksum_type("sha256"), cr.SHA256)
        self.assertEqual(cr.checksum_type("SHA256"), cr.SHA256)
        self.assertEqual(cr.checksum_type("Sha256"), cr.SHA256)
        self.assertEqual(cr.checksum_type("sHa256"), cr.SHA256)
        self.assertEqual(cr.checksum_type("ShA256"), cr.SHA256)

        self.assertEqual(cr.checksum_type("md5"), cr.MD5)
        self.assertEqual(cr.checksum_type("sha"), cr.SHA)
        self.assertEqual(cr.checksum_type("sha1"), cr.SHA1)
        self.assertEqual(cr.checksum_type("sha224"), cr.SHA224)
        self.assertEqual(cr.checksum_type("sha256"), cr.SHA256)
        self.assertEqual(cr.checksum_type("sha384"), cr.SHA384)
        self.assertEqual(cr.checksum_type("sha512"), cr.SHA512)

