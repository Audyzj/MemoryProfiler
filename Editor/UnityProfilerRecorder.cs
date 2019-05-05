using System;
using System.Collections.Generic;
using System.IO;
using UnityEditor;
using UnityEditorInternal;
using UnityEngine;
using UnityEngine.Profiling;

namespace Moobyte.MemoryProfiler
{
    internal struct StackSample
    {
        public int id;
        public string name;
        public int callsCount;
        public int gcAllocBytes;
        public float totalTime;
        public float selfTime;
    }
    
    public static class UnityProfilerRecorder
    {
        [MenuItem("性能/开始采样", false, 51)]
        public static void StartRecording()
        {
            var spacedir = string.Format("{0}/../ProfilerCapture", Application.dataPath);
            if (!Directory.Exists(spacedir))
            {
                Directory.CreateDirectory(spacedir);
            }

            if (stream != null)
            {
                stream.Close();
                stream = null;
            }

            {
                strmap = new Dictionary<string, int>();
                strseq = 0;
            }
            
            stream = new FileStream(string.Format("{0}/{1:yyyyMMddHHmmss}_PERF.pfc", spacedir, DateTime.Now), FileMode.Create);
            stream.Write('P'); // + 1
            stream.Write('F'); // + 1
            stream.Write('C'); // + 1
            stream.Write(DateTime.Now); // + 8
            stream.Write((uint)0); // + 4
            frameCount = 0;
            
            Profiler.enabled = true;
            EditorApplication.update -= Update;
            EditorApplication.update += Update;
        }

        [MenuItem("性能/停止采样", false, 52)]
        public static void StopRecording()
        {
            Profiler.enabled = false;
            EditorApplication.update -= Update;
            
            if (stream != null)
            {
                // encode strings
                var offset = (int)stream.Position;
                stream.Write(strmap.Count);
                
                var collection = new string[strmap.Count];
                foreach (var pair in strmap)
                {
                    collection[pair.Value] = pair.Key;
                }

                for (var i = 0; i < collection.Length; i++)
                {
                    stream.Write(collection[i]);
                }
                
                // encode string offset
                stream.Seek(11, SeekOrigin.Begin);
                stream.Write(offset);

                stream.Close();
                stream = null;
            }
        }

        internal static int getStringRef(string v)
        {
            int index = -1;
            if (!strmap.TryGetValue(v, out index))
            {
                strmap.Add(v, index = strseq++);
            }

            return index;
        }

        private static int frameCount = 0;
        private static FileStream stream;
        
        private static Dictionary<string, int> strmap;
        private static int strseq;
        
        static void Update()
        {
            var lastFrameIndex = ProfilerDriver.lastFrameIndex;
            if (lastFrameIndex >= 0)
            {
                if (frameCount++ >= 2)
                {
                    return;
                }

                var frameIndex = lastFrameIndex;

                var samples = new Dictionary<int, StackSample>();
                var relations = new Dictionary<int, int>();

                var cursor = new Stack<int>();
                var sequence = 0;
                
                var root = new ProfilerProperty();
                root.SetRoot(frameIndex, ProfilerColumn.TotalTime, ProfilerViewType.Hierarchy);
                root.onlyShowGPUSamples = false;
                while (root.Next(true))
                {
                    var depth = root.depth;
                    while (cursor.Count + 1 > depth)
                    {
                        cursor.Pop();
                    }
                    
                    samples.Add(sequence, new StackSample
                    {
                        id = sequence,
                        name = root.propertyName,
                        callsCount = (int)root.GetColumnAsSingle(ProfilerColumn.Calls),
                        gcAllocBytes = (int)root.GetColumnAsSingle(ProfilerColumn.GCMemory),
                        totalTime = root.GetColumnAsSingle(ProfilerColumn.TotalTime),
                        selfTime = root.GetColumnAsSingle(ProfilerColumn.SelfTime),
                    });

                    if (cursor.Count != 0)
                    {
                        relations[sequence] = cursor.Peek();
                    }

                    if (root.HasChildren)
                    {
                        cursor.Push(sequence);
                    }

                    ++sequence;
                }
                
                // frame_index
                stream.Write(frameIndex);
                stream.Write(float.Parse(root.frameTime));
                stream.Write(float.Parse(root.frameFPS));
                // samples
                stream.Write(samples.Count);
                foreach (var pair in samples)
                {
                    var v = pair.Value;
                    stream.Write(v.id);
                    stream.Write(getStringRef(v.name));
                    stream.Write(v.callsCount);
                    stream.Write(v.gcAllocBytes);
                    stream.Write(v.totalTime);
                    stream.Write(v.selfTime);
                }
                // relations
                stream.Write(relations.Count);
                foreach (var pair in relations)
                {
                    stream.Write(pair.Key);
                    stream.Write(pair.Value);
                }
                stream.Write((uint)0x12345678); // magic number
            }
        }
    }
}