# DESIGN DOC

## Flow

```mermaid
flowchart TD
  subgraph Student
    S_check[Check Assignment]
    S_submit[Submit Assignment]
    S_checkresult[Check Result]
  end
  subgraph Teacher
    T_publish[Publish Assignment]
    T_grade[Grade Assignment]
  end

  T_publish --> S_check
  S_check --> S_submit
  S_submit --> T_grade
  T_grade --> S_checkresult
```

## Features

- Complete flow of assignment submission, management and grading.
- Support computing AIGC rate of submitted assignment.
- Support plagiarism checking for assignments in a same assignment.
- Pretty visual graphs for teachers and students to check the status of
  assignment submission and grading.

## Architecture

HC-RE utilize C/S architecture.

### Frontend

- Student interface
  - Submit page
  - Assignments checking page
    - Details of assignments page
- Teacher interface
  - Assignment management page (Add, Edit, Delete)
  - Submission grading page
- Administrator interface
  - Server management page

In frontend (client), 

