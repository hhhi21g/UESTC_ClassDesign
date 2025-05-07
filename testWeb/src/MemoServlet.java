import Thymeleaf.viewBaseServlet;

import javax.servlet.ServletException;
import javax.servlet.annotation.WebServlet;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;
import javax.servlet.http.HttpSession;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

@WebServlet("/upload")
public class MemoServlet extends viewBaseServlet {
    @Override
    protected void doPost(HttpServletRequest req, HttpServletResponse resp) throws ServletException, IOException {
        HttpSession session = req.getSession();

        req.setCharacterEncoding("UTF-8");
        resp.setCharacterEncoding("UTF-8");
        resp.setContentType("text/html;charset=UTF-8");
        String content = req.getParameter("note");
        System.out.println(content);

        List<String> allContents = (List<String>) session.getAttribute("allContents");

        if (allContents == null) {
            allContents = new ArrayList<>();
        }

        allContents.add(content);
        session.setAttribute("allContents", allContents);
        req.setAttribute("allContents",allContents);
        System.out.println(allContents);
        super.processTemplate("index",req,resp);
    }
}
