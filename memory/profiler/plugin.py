from .crawler import MemorySnapshotCrawler, UnityManagedObject, JointConnection
from .core import PackedMemorySnapshot
from typing import List

class AnalyzePlugin(object):
    def __init__(self):
        self.crawler:MemorySnapshotCrawler = None
        self.snapshot:PackedMemorySnapshot = None
        self.args = []

    def setup(self, crawler:MemorySnapshotCrawler, *args):
        self.crawler = crawler
        self.snapshot = crawler.snapshot
        self.args = list(args)

    def analyze(self):
        pass

class ReferenceAnalyzer(AnalyzePlugin):
    def __init__(self):
        super().__init__()

    def analyze(self):
        import math
        managed_objects = self.crawler.managed_objects
        digit_count = math.ceil(math.log(len(managed_objects), 10))
        index_format = '[{:%dd}/%d]'%(int(digit_count), len(managed_objects))
        for n in range(len(managed_objects)):
            mo = managed_objects[n]
            if mo.is_value_type: continue
            object_type = self.snapshot.typeDescriptions[mo.type_index]
            print('{} 0x{:08x} object_type={} handle_index={}'.format(index_format.format(n+1) ,mo.address, object_type.name, mo.handle_index))
            print(self.crawler.dump_managed_object_reference_chain(object_index=mo.managed_object_index, indent=2))

class TypeNumberAnalyzer(AnalyzePlugin):
    def __init__(self):
        super().__init__()

    def analyze(self):
        pass


class TypeMemoryAnalyzer(AnalyzePlugin):
    def __init__(self):
        super().__init__()

    def analyze(self):
        pass

class StringAnalyzer(AnalyzePlugin):
    def __init__(self):
        super().__init__()

    def analyze(self):
        managed_strings = []
        string_type_index = self.crawler.snapshot.managedTypeIndex.system_String
        vm = self.crawler.snapshot.virtualMachineInformation
        for mo in self.crawler.managed_objects:
            if mo.type_index == string_type_index:
                managed_strings.append(mo)
        import operator
        managed_strings.sort(key=operator.attrgetter('size'))
        import math
        digit_format = '[{:%dd}/%d]' % (int(math.ceil(math.log(len(managed_strings), 10))), len(managed_strings))
        for n in range(len(managed_strings)):
            mo = managed_strings[n]
            data = self.crawler.heap_memory.read_string(address=mo.address + vm.objectHeaderSize)
            print('[String]{} 0x{:08x}={:,} {!r}'.format(digit_format.format(n+1), mo.address, mo.size, data))

class StaticAnalyzer(AnalyzePlugin):
    def __init__(self):
        super().__init__()

    def analyze(self):
        pass

class ScriptAnalyzer(AnalyzePlugin):
    def __init__(self):
        super().__init__()

    def analyze(self):
        pass

class DelegateAnalyzer(AnalyzePlugin):
    def __init__(self):
        super().__init__()

    def analyze(self):
        pass