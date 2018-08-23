package net.boxathome.waynes.mysvuat.main;

import android.os.Bundle;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.View;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends AppCompatActivity {

    static int haveDistance=1;
    static int haveInitial=2;
    static int haveFinal=4;
    static int haveAcceleration=8;
    static int haveTime=16;

    // All the "clear" buttons will be assigned to this one click handler
    // When pressed, they simply clear their associated EditText,
    // which is indicated by being the "nextFocusDown" element
    // described in the .xml file
    View.OnClickListener onClickListener = new View.OnClickListener() {

        @Override
        public void onClick(View v) {
            EditText et = findViewById(v.getNextFocusDownId());
            et.setText("", TextView.BufferType.EDITABLE); // wipe out the text
        }
    };


    View.OnClickListener compute = new View.OnClickListener() {

        @Override
        public void onClick(View v) {

            // check to see if exactly two text fields are empty
            // Start with SText and look at that plus the next 4 fields linked via
            // 'nextFocusDown'
            EditText et = findViewById(R.id.SText); // first one
            int empty = 0;
            int availableParams = 0;
            for (int q=0; q<5; q++) { // 5
                String s = et.getText().toString();
                if (s.isEmpty())
                    empty++;
                else
                    availableParams |= 1<<q;
                et = findViewById(et.getNextFocusDownId()); // follow link to next
            }

            if (empty != 2) {
                Toast.makeText(MainActivity.this, "Need exactly 2 empty fields", Toast.LENGTH_LONG).show();
                return;
            }

            // now we can perform our magic, and compute the empty fields based upon
            // the existing fields.
            // The bitfield availableParams has a bit set for each given parameter, so any
            // zero bits means that value needs to be computed.
            // For each zero, there will be two choices of which other variables can be used
            // to compute -- find the first one that has the necessary parameters
            Toast.makeText(MainActivity.this,"OK boss, working on it...", Toast.LENGTH_LONG).show();

            if ((availableParams & haveDistance) == 0) {
                // compute distance, one of the remaining 4 elements are missing, so don't use that formula
                if ((availableParams & haveInitial) == 0) {
                    //                   S = computeDist(V,A,T);
                }
                else if ((availableParams & haveFinal) == 0) {
                    //                 S = computeDist(U,A,T);
                }
            }

        }
    };




    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        // point all buttons to the same code, as which text element
        // to clear is assigned as their "nextFocusDown"
        ImageButton button = findViewById(R.id.clearSButton);
        button.setOnClickListener(onClickListener);
        button = findViewById(R.id.clearUButton);
        button.setOnClickListener(onClickListener);
        button = findViewById(R.id.clearVButton);
        button.setOnClickListener(onClickListener);
        button = findViewById(R.id.clearAButton);
        button.setOnClickListener(onClickListener);
        button = findViewById(R.id.clearTButton);
        button.setOnClickListener(onClickListener);

        FloatingActionButton fab = (FloatingActionButton) findViewById(R.id.fab);
        fab.setOnClickListener(compute);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }
}
