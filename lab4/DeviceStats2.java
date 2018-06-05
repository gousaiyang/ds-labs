import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.Map;

import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.fs.Path;
import org.apache.hadoop.io.DoubleWritable;
import org.apache.hadoop.io.LongWritable;
import org.apache.hadoop.io.Text;
import org.apache.hadoop.io.WritableComparable;
import org.apache.hadoop.io.WritableComparator;
import org.apache.hadoop.mapreduce.Job;
import org.apache.hadoop.mapreduce.Mapper;
import org.apache.hadoop.mapreduce.Reducer;
import org.apache.hadoop.mapreduce.lib.input.FileInputFormat;
import org.apache.hadoop.mapreduce.lib.output.FileOutputFormat;

public class DeviceStats2 {

    public static class DeviceMapper2 extends Mapper<LongWritable, Text, Text, DoubleWritable> {

        private Map<String, String> devMap = new HashMap<String, String>();

        public DeviceMapper2() throws FileNotFoundException, IOException {
            FileSystem fs = FileSystem.get(new Configuration());
            FSDataInputStream fin = fs.open(new Path("./preload/device.csv"));
            BufferedReader br = new BufferedReader(new InputStreamReader(fin));
            String strLine;

            while ((strLine = br.readLine()) != null) {
                String[] valueList = strLine.split(",");
                devMap.put(valueList[0], valueList[1]);
            }

            br.close();
        }

        public void map(LongWritable key, Text value, Context context) throws IOException, InterruptedException {
            String[] valueList = value.toString().split(",");

            if (valueList.length == 3 && devMap.containsKey(valueList[0])) {
                Long did = Long.parseLong(valueList[0]);

                if (did > 0 && did < 10 && !valueList[1].equals("NULL"))
                    context.write(new Text(valueList[1] + "\t" + devMap.get(valueList[0])), new DoubleWritable(Double.parseDouble(valueList[2])));
            }
        }
    }

    public static class DeviceReducer2 extends Reducer<Text, DoubleWritable, Text, DoubleWritable> {

        public void reduce(Text key, Iterable<DoubleWritable> values, Context context) throws IOException, InterruptedException {
            long length = 0;
            double sum = 0.0;

            for (DoubleWritable val : values) {
                sum += val.get();
                ++length;
            }

            sum /= length;

            context.write(key, new DoubleWritable(sum));
        }
    }

    public static class DescAscTextComparator extends WritableComparator {

        public DescAscTextComparator() {
            super(Text.class, true);
        }

        @SuppressWarnings("rawtypes")
        @Override
        public int compare(WritableComparable w1, WritableComparable w2) {
            String k1 = ((Text)w1).toString();
            String k2 = ((Text)w2).toString();

            String[] l1 = k1.split("\t");
            String[] l2 = k2.split("\t");

            int r1 = -1 * l1[0].compareTo(l2[0]);
            int r2 = l1[1].compareTo(l2[1]);

            return r1 != 0 ? r1 : r2;
        }
    }

    public static void main(String[] args) throws Exception {
        Configuration conf = new Configuration();
        Job job = Job.getInstance(conf, "device stats 2");
        job.setJarByClass(DeviceStats2.class);
        job.setMapperClass(DeviceMapper2.class);
        job.setCombinerClass(DeviceReducer2.class);
        job.setReducerClass(DeviceReducer2.class);
        job.setSortComparatorClass(DescAscTextComparator.class);
        job.setOutputKeyClass(Text.class);
        job.setOutputValueClass(DoubleWritable.class);
        FileInputFormat.addInputPath(job, new Path("./input"));
        FileOutputFormat.setOutputPath(job, new Path("./output"));
        System.exit(job.waitForCompletion(true) ? 0 : 1);
    }
}
